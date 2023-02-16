#include <os/irq.h>
#include <os/time.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/kernel.h>
#include <os/lock.h>
#include <os/smp.h>
#include <os/mm.h>
#include <pgtable.h>
#include <printk.h>
#include <assert.h>
#include <screen.h>

spin_lock_t kernel_spin_lock;

handler_t irq_table[IRQC_COUNT];
handler_t exc_table[EXCC_COUNT];

/**
 * The following is from the RISC-V PRIVILEGED MANUAL.
 * 
 * When a hardware breakpoint is triggered, or an instruction-fetch, load, or store access or page-fault
 * exception occurs, or an instruction-fetch or AMO address-misaligned exception occurs, stval is
 * written with the faulting address.
 * For other exceptions, stval is set to zero, but a future standard
 * may redefine stval's setting for other exceptions.
 **/

extern void ret_from_exception();

void interrupt_helper(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // every time a processor wants to enter kernel
    // it must first acquire the kernel spin lock
    lock_kernel();

    // store tp(current_running pcb) to variable current_running
    // here, current_running == runnings[cpuid] -> true
    current_running = runnings[get_current_cpu_id()];

    // call corresponding handler by the value of `scause`
    if (scause & 0x8000000000000000L) { /* Interrupt bit is 1 */
        scause &= SCAUSE_MASK;
        irq_table[scause](regs, stval, scause);
    } else {                            /* Interrupt bit is 0  */
        exc_table[scause](regs, stval, scause);
    }

    ret_from_kernel();
}

void handle_irq_timer(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // clock interrupt handler.
    // use bios_set_timer to reset the timer and remember to reschedule
    set_timer(get_ticks() + TIMER_INTERVAL);
    do_scheduler();
}

void handle_page_fault(regs_context_t *regs, uint64_t stval, uint64_t scause) {
    PTE *pgdir = current_running->pgdir;

    uint64_t fault_addr_uva = stval & (~((1 << NORMAL_PAGE_SHIFT)-1));
    uint64_t fault_addr_kva = get_kva_of_uva(fault_addr_uva, current_running->pgdir);

    // uva not allocated & mapped
    if (fault_addr_kva == 0) {
        alloc_page_helper(fault_addr_uva, pgdir);
    } else {
        page_t *page = find_page_with_uva(fault_addr_uva, pgdir, &swapped_pages_queue);
        if (present_pages_num >= MAX_PRESENT_PFN) {
            swap_out();
        }
        swap_in(page);
    }

    PTE *pte = get_pte_of_uva(fault_addr_uva, pgdir);
    set_attribute(pte, _PAGE_ACCESSED);
    if (scause == EXCC_STORE_PAGE_FAULT) {
        set_attribute(pte, _PAGE_DIRTY);
    }
}

void handle_other(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    char* reg_name[] = {
        "zero "," ra  "," sp  "," gp  "," tp  ",
        " t0  "," t1  "," t2  ","s0/fp"," s1  ",
        " a0  "," a1  "," a2  "," a3  "," a4  ",
        " a5  "," a6  "," a7  "," s2  "," s3  ",
        " s4  "," s5  "," s6  "," s7  "," s8  ",
        " s9  "," s10 "," s11 "," t3  "," t4  ",
        " t5  "," t6  "
    };
    for (int i = 0; i < 32; i += 3) {
        for (int j = 0; j < 3 && i + j < 32; ++j) {
            printk("%s : %016lx ",reg_name[i+j], regs->regs[i+j]);
        }
        printk("\n\r");
    }
    printk("sstatus: 0x%lx sbadaddr: 0x%lx scause: %lu\n\r",
           regs->sstatus, regs->sbadaddr, regs->scause);
    printk("sepc: 0x%lx\n\r", regs->sepc);
    printk("tval: 0x%lx cause: 0x%lx\n", stval, scause);
    assert(0);
}

void init_exception()
{
    /* initialize exc_table */
    /* NOTE: handle_syscall, handle_other, etc.*/
    for (int code = 0; code < EXCC_COUNT; code++) {
        exc_table[code] = handle_other;
    }
    exc_table[EXCC_SYSCALL]          = handle_syscall;
    exc_table[EXCC_INST_PAGE_FAULT]  = handle_page_fault;
    exc_table[EXCC_LOAD_PAGE_FAULT]  = handle_page_fault;
    exc_table[EXCC_STORE_PAGE_FAULT] = handle_page_fault;

    /* initialize irq_table */
    /* NOTE: handle_int, handle_other, etc.*/
    for (int code = 0; code < IRQC_COUNT; code++) {
        irq_table[code] = handle_other;
    }
    irq_table[IRQC_S_TIMER] = handle_irq_timer;

    /* set up the entrypoint of exceptions */
    setup_exception();
    set_timer(get_ticks() + TIMER_INTERVAL);
}
