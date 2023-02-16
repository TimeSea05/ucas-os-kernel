#include <atomic.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/lock.h>
#include <os/kernel.h>

extern spin_lock_t kernel_spin_lock;

void smp_init()
{
    spin_lock_acquire(&kernel_spin_lock);
}

void wakeup_other_hart()
{
    send_ipi(NULL);
    asm volatile("csrw sip, zero");
}

void lock_kernel()
{
    while (spin_lock_try_acquire(&kernel_spin_lock) == LOCKED);
}

void unlock_kernel()
{
    spin_lock_release(&kernel_spin_lock);
}

void slave_wait_for_task() {
    // at the initial stage, only master processor is carrying out tasks
    // therefore, slave processor should wait until
    // there are more than 2 tasks in ready_queue
    // which means ready_queue.next->next != &ready_queue
    while (1) {
        if (is_queue_empty(&ready_queue) || ready_queue.next->next == &ready_queue) {
            unlock_kernel();                             // less than 2 tasks
        } else {                                         // 2 or more tasks
            runnings[1] = list_entry(ready_queue.next->next, pcb_t);
            current_running = runnings[1];
            current_running->status = TASK_RUNNING;
            break;
        }
        lock_kernel();
    }
}