#include <common.h>
#include <asm.h>
#include <pgtable.h>
#include <asm/unistd.h>
#include <os/loader.h>
#include <os/irq.h>
#include <os/sched.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/mm.h>
#include <os/time.h>
#include <os/smp.h>
#include <os/ioremap.h>
#include <os/net.h>
#include <os/fs.h>
#include <sys/syscall.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <e1000.h>
#include <type.h>
#include <csr.h>

// Task info array
task_info_t tasks[TASK_MAXNUM];

// App info
int kernel_file_sz;
int total_sectors_num;
int tasks_num;

int kernel_sectors_num;
int disk_sectors_startid;

static void init_jmptab(void);
static void init_task_info(void);
static void init_pcb(void);
static void init_syscall(void);
static void unmap_boot_page();

int main(void)
{
    lock_kernel();
    unmap_boot_page();

    int cpuid = get_current_cpu_id();
    if (cpuid == 0) {   // master processor
        wakeup_other_hart();
        init_jmptab();
        init_task_info();
        init_kernel_freemem();

        init_pcb();
        printk("> [INIT] PCB initialization succeeded.\n");

        // Read CPU frequency
        time_base = bios_read_fdt(TIMEBASE);

        // FIXME: Please uncomment the following code after implementing file system
        // Read E1000 Registers Base Pointer
        e1000 = (volatile uint8_t *)bios_read_fdt(EHTERNET_ADDR);
        uint64_t plic_addr = bios_read_fdt(PLIC_ADDR);
        uint32_t nr_irqs = (uint32_t)bios_read_fdt(NR_IRQS);
        printk("> [INIT] e1000: 0x%lx, plic_addr: 0x%lx, nr_irqs: 0x%lx.\n", e1000, plic_addr, nr_irqs);

        // IO remap
        e1000 = (uint8_t *)ioremap((uint64_t)e1000, 8 * NORMAL_PAGE_SIZE);
        printk("> [INIT] IOremap initialization succeeded.\n");

        init_locks();
        init_barriers();
        init_conditions();
        init_mbox();
        printk("> [INIT] Lock mechanism initialization succeeded.\n");

        init_exception();
        printk("> [INIT] Interrupt processing initialization succeeded.\n");

        // FIXME: Please uncomment the following code after implementing file system
        // Init network device
        e1000_init();
        printk("> [INIT] E1000 device initialized successfully.\n");

        init_syscall();
        printk("> [INIT] System call initialized successfully.\n");

        init_screen();
        printk("> [INIT] SCREEN initialization succeeded.\n");

        share_pgtable(current_running->pgdir, (PTE *)(PGDIR_VA));
        init_superblock();
    } else {            // slave processor
        setup_exception();
        set_timer(get_ticks() + TIMER_INTERVAL);
        
        slave_wait_for_task();
    }
                
    printk("> [CPU] CPU ID: %d\n", cpuid);
    // after switching context, kernel is unlocked
    if (cpuid == 0) {
        switch_to(&pid0_pcb, current_running); 
    } else if (cpuid == 1) {
        switch_to(&pid0_pcb2, current_running);
    }

    asm volatile("ld tp, %0"::"m"(current_running));
    unlock_kernel();
    enable_interrupt();
    set_timer(get_ticks() + TIMER_INTERVAL);
    while (1) {
        enable_preempt();
        asm volatile("wfi");
    }

    return 0;
}

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
    jmptab[SD_WRITE]        = (long (*)())sd_write;
    jmptab[QEMU_LOGGING]    = (long (*)())qemu_logging;
    jmptab[SET_TIMER]       = (long (*)())set_timer;
    jmptab[READ_FDT]        = (long (*)())read_fdt;
    jmptab[MOVE_CURSOR]     = (long (*)())screen_move_cursor;
    jmptab[PRINT]           = (long (*)())printk;
}

static void init_task_info(void)
{
    int *app_info_entry = (int *)APP_INFO_ENTRY;

    // load app info from bootblock
    kernel_file_sz       = *app_info_entry++;
    tasks_num            = *app_info_entry++;
    total_sectors_num    = *app_info_entry++;

    kernel_sectors_num   = NBYTES2SEC(kernel_file_sz);
    disk_sectors_startid = total_sectors_num + 1;         // 1: task_info array

    // load task_info array into memory
    task_info_t *task_info_array = (void *)0xffffffc050800000;
    bios_sdread((long)kva2pa((uintptr_t)task_info_array), 1, total_sectors_num);
    memcpy((uint8_t *)tasks, (uint8_t *)task_info_array, tasks_num * sizeof(task_info_t));

#ifdef QEMU
    // in qemu, load all task sectors into memory
    load_all_tasks_qemu();
#endif
}

static void init_pcb(void)
{
    pid0_pcb.kernel_sp  = (uint64_t)kalloc()  + PAGE_SIZE - sizeof(regs_context_t);
    pid0_pcb2.kernel_sp = (ptr_t)kalloc() + PAGE_SIZE - sizeof(regs_context_t);
    
    strcpy(pid0_pcb.name, "pid0_pcb");
    strcpy(pid0_pcb2.name, "pid0_pcb2");

    for (int i = tasks_num; i < 16; i++) pcb[i].pid = 0;

    pcb_t *p  = create_pcb("shell");
    p->status = TASK_RUNNING;

    // only cpu0(master processor) can initialize current_running
    runnings[0] = list_entry(ready_queue.next, pcb_t);
    current_running = runnings[0];
}

static void init_syscall(void)
{
    // initialize system call table.
    syscall[SYSCALL_EXIT]           = (long (*)())do_exit;
    syscall[SYSCALL_EXEC]           = (long (*)())do_exec;
    syscall[SYSCALL_SLEEP]          = (long (*)())do_sleep;
    syscall[SYSCALL_KILL]           = (long (*)())do_kill;
    syscall[SYSCALL_WAITPID]        = (long (*)())do_waitpid;
    syscall[SYSCALL_PS]             = (long (*)())do_process_show;
    syscall[SYSCALL_GETPID]         = (long (*)())do_getpid;
    syscall[SYSCALL_YIELD]          = (long (*)())do_scheduler;

    syscall[SYSCALL_WRITE]          = (long (*)())screen_write;
    syscall[SYSCALL_READCH]         = (long (*)())bios_getchar;
    syscall[SYSCALL_CURSOR]         = (long (*)())screen_move_cursor;
    syscall[SYSCALL_REFLUSH]        = (long (*)())screen_reflush;
    syscall[SYSCALL_CLEAR]          = (long (*)())screen_clear;
    syscall[SYSCALL_BACKSPACE]      = (long (*)())screen_backspace;

    syscall[SYSCALL_GET_TIMEBASE]   = (long (*)())get_time_base;
    syscall[SYSCALL_GET_TICK]       = (long (*)())get_ticks;

    syscall[SYSCALL_THREAD_CREATE]  = (long (*)())do_thread_create;
    syscall[SYSCALL_THREAD_JOIN]    = (long (*)())do_thread_join;

    syscall[SYSCALL_LOCK_INIT]      = (long (*)())do_mutex_lock_init;
    syscall[SYSCALL_LOCK_ACQ]       = (long (*)())do_mutex_lock_acquire;
    syscall[SYSCALL_LOCK_RELEASE]   = (long (*)())do_mutex_lock_release;

    syscall[SYSCALL_BARR_INIT]      = (long (*)())do_barrier_init;
    syscall[SYSCALL_BARR_WAIT]      = (long (*)())do_barrier_wait;
    syscall[SYSCALL_BARR_DESTROY]   = (long (*)())do_barrier_destroy;
    
    syscall[SYSCALL_COND_INIT]      = (long (*)())do_condition_init;
    syscall[SYSCALL_COND_WAIT]      = (long (*)())do_condition_wait;
    syscall[SYSCALL_COND_SIGNAL]    = (long (*)())do_condition_signal;
    syscall[SYSCALL_COND_BROADCAST] = (long (*)())do_condition_broadcast;
    syscall[SYSCALL_COND_DESTROY]   = (long (*)())do_condition_destroy;

    syscall[SYSCALL_MBOX_OPEN]      = (long (*)())do_mbox_open;
    syscall[SYSCALL_MBOX_CLOSE]     = (long (*)())do_mbox_close;
    syscall[SYSCALL_MBOX_SEND]      = (long (*)())do_mbox_send;
    syscall[SYSCALL_MBOX_RECV]      = (long (*)())do_mbox_recv;

    syscall[SYSCALL_SHM_GET]        = (long (*)())shm_page_get;
    syscall[SYSCALL_SHM_DT]         = (long (*)())shm_page_dt;

    syscall[SYSCALL_NET_SEND]       = (long (*)())do_net_send;
    syscall[SYSCALL_NET_RECV]       = (long (*)())do_net_recv;

    syscall[SYSCALL_FS_MKFS]        = (long (*)())do_mkfs;
    syscall[SYSCALL_FS_STATFS]      = (long (*)())do_statfs;
    syscall[SYSCALL_FS_MKDIR]       = (long (*)())do_mkdir;
    syscall[SYSCALL_FS_LS]          = (long (*)())do_ls;
    syscall[SYSCALL_FS_CD]          = (long (*)())do_cd;
    syscall[SYSCALL_FS_RMDIR]       = (long (*)())do_rmdir;
    syscall[SYSCALL_FS_TOUCH]       = (long (*)())do_touch;
    syscall[SYSCALL_FS_FOPEN]       = (long (*)())do_fopen;
    syscall[SYSCALL_FS_FCLOSE]      = (long (*)())do_fclose;
    syscall[SYSCALL_FS_FREAD]       = (long (*)())do_fread;
    syscall[SYSCALL_FS_FWRITE]      = (long (*)())do_fwrite;
    syscall[SYSCALL_FS_CAT]         = (long (*)())do_cat;
    syscall[SYSCALL_FS_LN]          = (long (*)())do_ln;
    syscall[SYSCALL_FS_RM]          = (long (*)())do_rm;
    syscall[SYSCALL_FS_LSEEK]       = (long (*)())do_lseek;
}

// clear the mapping of boot address
// mapping: 0x50000000-0x51000000 => 0x50000000-0x51000000
// this function is called by kernel, so all of the addresses
// used in this function should be kernel virtual address
static void unmap_boot_page() {
    uint64_t vpn2 = 1;
    PTE *pgdir = (PTE *)PGDIR_VA;
    PTE *pmd   = (PTE *)pa2kva(get_pa(pgdir[vpn2]));

    clear_pgdir((uintptr_t)pmd);
    pgdir[vpn2] = 0;
}