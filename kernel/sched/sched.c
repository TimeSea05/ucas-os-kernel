#include <os/list.h>
#include <os/task.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/loader.h>
#include <os/mm.h>
#include <os/smp.h>
#include <os/string.h>
#include <os/net.h>
#include <pgtable.h>
#include <csr.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>

#define STACK_PAGE_NUM 1

extern void ret_from_exception();

pcb_t pcb[NUM_MAX_TASK];
int threads[NUM_MAX_SUB_THREADS];

int tcb_id = 0;

pcb_t pid0_pcb  = { .pid = 0, .pgdir = (PTE *)PGDIR_VA };
pcb_t pid0_pcb2 = { .pid = 0, .pgdir = (PTE *)PGDIR_VA };

extern mutex_lock_t mlocks[LOCK_NUM];

const char *status[] = {"BLOCKED", "RUNNING", "READY", "EXITED"};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

// current running pcbs(for multicores)
pcb_t *volatile runnings[NR_CPUS];

/* current running task PCB */
pcb_t * volatile current_running;

/* global process id */
pid_t process_id = 1;


// switch to the pagetable of user process
// then flush TLB
void switch_pgdir() {
    set_satp(SATP_MODE_SV39, current_running->pid,
             kva2pa((uintptr_t)current_running->pgdir) >> NORMAL_PAGE_SHIFT);
    local_flush_tlb_all();
}

void ret_from_kernel() {
    asm volatile ("ld tp, %0": :"m"(current_running));
    
    switch_pgdir();
    unlock_kernel();
    ret_from_exception();
}

void find_idle_task() {
    int cpuid = get_current_cpu_id();
    
    // try to find a idle pcb(task), set current_running to it
    pcb_t *idle_pcb = cpuid == 0 ? &pid0_pcb : &pid0_pcb2;
    
    // ready_queue empty: wait until not empty
    // this occurs when no task in running on all cpus
    // then all cpus are traped in this loop
    if (is_queue_empty(&ready_queue)) {
        goto finish;
    }

    // go through ready_queue
    pcb_t *p;
    list_for_each_entry(p, &ready_queue) {
        if (p->status == TASK_RUNNING) continue;
        idle_pcb = p;
        break;
    }
        
    finish:
        current_running = idle_pcb;
        current_running->status = TASK_RUNNING;
        runnings[cpuid] = current_running;

        swap_in_all_pages(current_running->pgdir);
}

void do_scheduler(void)
{
    // Check sleep queue to wake up PCBs
    check_sleeping();
    
    // Check send/recv queue to unblock PCBs
    // FIXME: Please uncomment the following code after implementing file system
    check_net_send();
    check_net_recv();

    // Modify the current_running pointer.
    pcb_t *before_running = current_running;
    before_running->status = TASK_READY;

    // check if before_running is pcb0(bubble)
    if (before_running != &pid0_pcb && before_running != &pid0_pcb2) {
        list_delete_entry(&before_running->list);
        list_add_tail(&before_running->list, &ready_queue);
    }

    // current_running is modified in function `find_idle_task`
    find_idle_task();
    switch_pgdir();
    switch_to(before_running, current_running);
}

void do_sleep(uint32_t sleep_time)
{
    // sleep(seconds)
    // assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.
    pcb_t *gonna_sleep = current_running;
    gonna_sleep->wakeup_time = get_timer() + sleep_time;

    do_block(&gonna_sleep->list, &sleep_queue);
}

void do_block(list_node_t *pcb_node, list_head *queue)
{
    // block the pcb task into the block queue
    list_delete_entry(pcb_node);
    list_add_tail(pcb_node, queue);

    pcb_t *before_running = list_entry(pcb_node, pcb_t);
    before_running->status = TASK_BLOCKED;

    find_idle_task();
    switch_pgdir();
    switch_to(before_running, current_running);
}

void do_unblock(list_node_t *pcb_node)
{
    // unblock the `pcb` from the block queue
    list_delete_entry(pcb_node);

    pcb_t *wakeup = list_entry(pcb_node, pcb_t);
    wakeup->status = TASK_READY;
    list_add_tail(pcb_node, &ready_queue);
}

void do_process_show() {
    printk("[Process Table]:\n");

    for (int i = 0; i < NUM_MAX_TASK; i++) {
        if (pcb[i].pid != 0) {
            printk("[%d] NAME: %s  PID: %d  STATUS: %s\n", i, pcb[i].name, pcb[i].pid, status[pcb[i].status]);
        }
    }
}

static pcb_t *find_unused_pcb() {
    for (int i = 0; i < NUM_MAX_TASK; i++) {
        // find an unused pcb
        if (pcb[i].pid == 0) {
            return &pcb[i];
            break;
        }
    }

    return NULL;
}

static void init_pcb_context(pcb_t *p) {
    ptr_t kernel_stack      = (ptr_t)kalloc() + PAGE_SIZE;
    ptr_t user_stack        = alloc_page_helper(USER_VA_SP_BASE, (PTE *)p->pgdir) + PAGE_SIZE;

    // stack page should not hold attribute EXEC
    PTE *user_stack_pte = get_pte_of_uva(USER_VA_SP_BASE, p->pgdir);
    unset_attribute(user_stack_pte, _PAGE_EXEC);

    regs_context_t *pt_regs = (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    pt_regs->regs[2] = USER_VA_SP;  // sp
    pt_regs->regs[4] = (ptr_t)p;    // tp
    pt_regs->sepc = USER_VA_START;

    pt_regs->sstatus &= ~SR_SPP;        /* clear SPP to 0 for user mode */
    pt_regs->sstatus |= SR_SPIE;        /* enable interrupts in user mode */
    pt_regs->sstatus |= SR_SUM;

    switchto_context_t *pt_switchto = (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
    pt_switchto->regs[0] = (ptr_t)ret_from_kernel;  // ra
    pt_switchto->regs[1] = (ptr_t)pt_regs;          // sp
        
    p->kernel_sp        = (ptr_t)pt_regs;
    p->user_sp          = user_stack;
    p->trapframe        = pt_regs;
    p->switchto_context = pt_switchto;
}

pcb_t *create_pcb(char *name) {
    // try to find unused pcb
    pcb_t *p = find_unused_pcb();
    // if cannot find an unused pcb, return NULL for failure
    if (p == NULL) return NULL;

    int task_found = 0;
    for (int i = 0; i < NUM_MAX_TASK; i++) {
        if (strcmp(name, tasks[i].name) == 0) {
            task_found = 1;

            p->pid    = process_id++;
            p->tid    = ++(threads[p->pid]);
            p->status = TASK_READY;
            strcpy(p->name, name);
            list_add_tail(&p->list, &ready_queue);
            INIT_LIST_HEAD(&p->wait_list);

            // allocate a page as pagetable for this process
            // then read the SD card, copy the content of SD card
            // to this pagetable
            p->pgdir = (PTE *)kalloc();
            assert(load_task_img(tasks[i].name, (PTE *)p->pgdir) == 1);

            // map kernel pagetable to user pagetable
            share_pgtable(p->pgdir, (PTE *)PGDIR_VA);

            // initialize stacks(kernel & user) and
            // contexts(trapframe & switchto) of pcb
            init_pcb_context(p);
            break;
        }
    }

    // cannot find a task from tasks array with given name
    // return NULL for failure
    if (task_found == 0) return NULL;

    // successfully created a pcb
    // return the address the pcb
    return p;
}


pid_t do_exec(char *name, int argc, char *argv[]) {
    pcb_t *p = create_pcb(name);
    
    // create pcb failed, return 0 for failure
    if (p == NULL) return 0;

    /* set user stack, including argc, argv[] */
    char *user_sp_kva = (char *)p->user_sp;
    char *user_sp_uva = (char *)USER_VA_SP;

    ptr_t *kernel_argv_base = (ptr_t *)(user_sp_kva - (argc+1)*sizeof(ptr_t));
    uint64_t user_argv_base = USER_VA_SP - (argc+1)*sizeof(ptr_t);
    
    ptr_t *kernel_argv_ptr = kernel_argv_base;

    user_sp_kva = (char *)kernel_argv_base;
    user_sp_uva = (char *)user_argv_base;

    for (int i = 0; i < argc; i++, kernel_argv_ptr++) {
        int len = strlen(argv[i]) + 1;                  // plus '\0'
        user_sp_kva -= len, user_sp_uva -= len;
        *kernel_argv_ptr = (ptr_t)user_sp_uva;          // calculate the value of argv[i]
        
        for (int j = 0; j < len; j++) {
            *(user_sp_kva + j) = *(argv[i] + j);        // copy string to user stack
        }
    }
    *kernel_argv_ptr++ = 0;                             // argv[]: null-terminate

    user_sp_uva = (char *)ROUNDDOWN(user_sp_uva, 128);
    regs_context_t *trapframe = (regs_context_t *)(p->kernel_sp);
    trapframe->regs[2] = (reg_t)user_sp_uva;            // update sp in trapframe
    trapframe->regs[10] = (reg_t)argc;                  // set a0 to argc
    trapframe->regs[11] = (reg_t)user_argv_base;        // set a1 to argv_base

    p->cwd_inum = current_running->cwd_inum;

    return p->pid;
}

static void release_pcb(pcb_t *p) {
    p->status = TASK_EXITED;
    threads[p->pid]--;

    // remove the task from list(ready_queue, block_queue, etc);
    list_delete_entry(&p->list);

    // release all the locks that exited is holding
    for (int i = 0; i < LOCK_NUM; i++) {
        if (mlocks[i].pid == p->pid) {
            do_mutex_lock_release(i);
        }
    }

    // wakeup processes that are waiting for current_running to exit
    list_head *wait_list = &p->wait_list;
    while (!is_queue_empty(wait_list)) {
        do_unblock(wait_list->next);
    }
}

void do_exit(void) {
    // release current_running
    pcb_t *exited = current_running;
    release_pcb(exited);

    // if no threads of current process is running
    // free pagetable, page directory and then switch pagetable
    // else, only free the user stack page allocated for the thread
    if (threads[exited->pid] == 0) {
        free_pagetable(exited->pgdir);

        find_idle_task();
        switch_pgdir();
        free_pgdir(exited->pgdir);
    } else {
        uint64_t thread_stack_base_kva = exited->user_sp - PAGE_SIZE;
        free_page_with_kva(thread_stack_base_kva);
    }

    exited->pid = 0;
    switch_to(exited, current_running);
}

int do_kill(pid_t pid) {
    pcb_t *killed = NULL;
    for (int i = 0; i < NUM_MAX_TASK; i++) {
        if (pcb[i].pid == pid) {
            killed = &pcb[i];
            break;
        }
    }

    if (killed == NULL) return 0;

    release_pcb(killed);

    // if no threads of current process is running
    // free pagetable, page directory and then switch pagetable
    // else, only free the user stack page allocated for the thread
    if (threads[killed->pid] == 0) {
        free_pagetable(killed->pgdir);
        free_pgdir(killed->pgdir);
    } else {
        uint64_t thread_stack_base_kva = killed->user_sp - PAGE_SIZE;
        free_page_with_kva(thread_stack_base_kva);
    }

    killed->pid = 0;
    return 1;
}

pthread_t do_thread_create(long start_routine, long arg) {
    pcb_t *p = find_unused_pcb();
    if (p == NULL) return 0;

    pcb_t *main_thread = current_running;
    p->pid    = main_thread->pid;
    p->tid    = ++threads[p->pid];
    p->status = TASK_READY;

    strcpy(p->name, main_thread->name);
    list_add_tail(&p->list, &ready_queue);
    INIT_LIST_HEAD(&p->wait_list);

    // all threads of a process share pagetable
    p->pgdir = main_thread->pgdir;

    ptr_t kernel_stack  = (ptr_t)kalloc() + PAGE_SIZE;
    ptr_t user_stack_uva = USER_VA_SP_BASE + (p->tid - 1) * PAGE_SIZE;
    ptr_t user_stack    = alloc_page_helper(user_stack_uva, p->pgdir) + PAGE_SIZE;

    // stack page should not hold attribute EXEC
    PTE *user_stack_pte = get_pte_of_uva(user_stack_uva, p->pgdir);
    unset_attribute(user_stack_pte, _PAGE_EXEC);

    regs_context_t *pt_regs = (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    pt_regs->regs[2] = user_stack_uva + PAGE_SIZE;  // sp
    pt_regs->regs[4] = (ptr_t)p;                   // tp
    pt_regs->regs[10] = (reg_t)arg;                // set a0 to arg
    pt_regs->sepc = start_routine;

    pt_regs->sstatus &= ~SR_SPP;        /* clear SPP to 0 for user mode */
    pt_regs->sstatus |= SR_SPIE;        /* enable interrupts in user mode */
    pt_regs->sstatus |= SR_SUM;

    switchto_context_t *pt_switchto = (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
    pt_switchto->regs[0] = (ptr_t)ret_from_kernel;  // ra
    pt_switchto->regs[1] = (ptr_t)pt_regs;          // sp
        
    p->kernel_sp        = (ptr_t)pt_regs;
    p->user_sp          = user_stack;
    p->trapframe        = pt_regs;
    p->switchto_context = pt_switchto;

    return p->tid;
}

pthread_t do_thread_join(pthread_t thread) {
    pcb_t *p, *p_q;
    list_for_each_entry_safe(p, p_q, &ready_queue) {
        if (p->pid == current_running->pid && p->tid == thread) {
            do_block(&current_running->list, &p->wait_list);
        }
    }

    return thread;
}

pid_t do_getpid() {
    return runnings[get_current_cpu_id()]->pid;
}

int do_waitpid(pid_t pid) {
    pcb_t *to_wait = NULL;

    for (int i = 0; i < NUM_MAX_TASK; i++) {
        if (pcb[i].pid == pid) {
            to_wait = &pcb[i];
            break;
        }
    }

    if (to_wait == NULL) return 0;

    do_block(&current_running->list, &to_wait->wait_list);
    return to_wait->pid;
}
