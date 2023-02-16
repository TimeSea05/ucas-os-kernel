#include <asm.h>
#include <csr.h>
#include <asm/regs.h>

.macro SAVE_CONTEXT
  /* save all general purpose registers here! */
  csrrw a0, sscratch, a0     /* swap the value of a0 and sscratch */

  mv a0, tp                   /* load the address of current_running(pcb_t *) to a0 */
  ld a0, PCB_KERNEL_SP(a0)    /* load the value of kernel_sp to a0 */

  /* pointers */
  sd ra, OFFSET_REG_RA(a0)
  sd sp, OFFSET_REG_SP(a0)
  sd tp, OFFSET_REG_TP(a0)
  
  /* temporary registers */
  sd t0, OFFSET_REG_T0(a0)
  sd t1, OFFSET_REG_T1(a0)
  sd t2, OFFSET_REG_T2(a0)
  
  /* saved registers */
  sd s0, OFFSET_REG_S0(a0)
  sd s1, OFFSET_REG_S1(a0)
  
  /* args */
  /* a0: now used as indirect addressing register */
  sd a1, OFFSET_REG_A1(a0)
  sd a2, OFFSET_REG_A2(a0)
  sd a3, OFFSET_REG_A3(a0)
  sd a4, OFFSET_REG_A4(a0)
  sd a5, OFFSET_REG_A5(a0)
  sd a6, OFFSET_REG_A6(a0)
  sd a7, OFFSET_REG_A7(a0)

  /* saved registers */
  sd s2, OFFSET_REG_S2(a0)
  sd s3, OFFSET_REG_S3(a0)
  sd s4, OFFSET_REG_S4(a0)
  sd s5, OFFSET_REG_S5(a0)
  sd s6, OFFSET_REG_S6(a0)
  sd s7, OFFSET_REG_S7(a0)
  sd s8, OFFSET_REG_S8(a0)
  sd s9, OFFSET_REG_S9(a0)
  sd s10, OFFSET_REG_S10(a0)
  sd s11, OFFSET_REG_S11(a0)

  /* temporary registers */
  sd t3, OFFSET_REG_T3(a0)
  sd t4, OFFSET_REG_T4(a0)
  sd t5, OFFSET_REG_T5(a0)
  sd t6, OFFSET_REG_T6(a0)

  csrr t0, sscratch          /* retrieve the original value from sscratch */
  sd t0, OFFSET_REG_A0(a0)

  mv sp, a0                  /* change from user stack to kernel stack */

  /* save sstatus, sepc, stval and scause on kernel stack */

  /* control registers */
  csrr t0, sstatus
  sd t0, OFFSET_REG_SSTATUS(sp)
  /*
   * Disable user-mode memory access as it should only be set in the
   * actual user copy routines.
   *
   * Disable the FPU to detect illegal usage of floating point in kernel
   * space.
   */
  li t1, SR_SUM | SR_FS
  or t0, t0, t1
  csrw sstatus, t0

  csrr t0, sepc
  sd t0, OFFSET_REG_SEPC(sp)
  csrr t0, sbadaddr
  sd t0, OFFSET_REG_SBADADDR(sp)
  csrr t0, scause
  sd t0, OFFSET_REG_SCAUSE(sp)

  addi sp, sp, -SWITCH_TO_SIZE
.endm

.macro RESTORE_CONTEXT
  /* Restore all general purpose registers and sepc, sstatus */
  
  mv a0, tp                   /* load the address of current_running(pcb_t *) to a0 */
  ld a0, PCB_KERNEL_SP(a0)    /* load the value of kernel_sp to a0 */

  ld t0, OFFSET_REG_A0(a0)    /* load user a0 to t0 */
  csrw sscratch, t0           /* store user a0 in sscratch */

  /* restore sepc and sstatus */
  ld t0, OFFSET_REG_SEPC(a0)
  csrw sepc, t0

  ld t0, OFFSET_REG_SSTATUS(a0)
  andi t0, t0, -256          /* clear SPP to 0 for user mode */
  ori t0, t0, SR_SPIE        /* enable interrupts in user mode */
  csrw sstatus, t0

  /* pointers */
  ld ra, OFFSET_REG_RA(a0)
  ld sp, OFFSET_REG_SP(a0)
  ld tp, OFFSET_REG_TP(a0)
  
  /* temporary registers */
  ld t0, OFFSET_REG_T0(a0)
  ld t1, OFFSET_REG_T1(a0)
  ld t2, OFFSET_REG_T2(a0)
  
  /* saved registers */
  ld s0, OFFSET_REG_S0(a0)
  ld s1, OFFSET_REG_S1(a0)
  
  /* args */
  /* a0: now used as indirect addressing register */
  ld a1, OFFSET_REG_A1(a0)
  ld a2, OFFSET_REG_A2(a0)
  ld a3, OFFSET_REG_A3(a0)
  ld a4, OFFSET_REG_A4(a0)
  ld a5, OFFSET_REG_A5(a0)
  ld a6, OFFSET_REG_A6(a0)
  ld a7, OFFSET_REG_A7(a0)

  /* saved registers */
  ld s2, OFFSET_REG_S2(a0)
  ld s3, OFFSET_REG_S3(a0)
  ld s4, OFFSET_REG_S4(a0)
  ld s5, OFFSET_REG_S5(a0)
  ld s6, OFFSET_REG_S6(a0)
  ld s7, OFFSET_REG_S7(a0)
  ld s8, OFFSET_REG_S8(a0)
  ld s9, OFFSET_REG_S9(a0)
  ld s10, OFFSET_REG_S10(a0)
  ld s11, OFFSET_REG_S11(a0)

  /* temporary registers */
  ld t3, OFFSET_REG_T3(a0)
  ld t4, OFFSET_REG_T4(a0)
  ld t5, OFFSET_REG_T5(a0)
  ld t6, OFFSET_REG_T6(a0)

  csrrw a0, sscratch, a0     /* restore a0 from sscratch */
.endm

ENTRY(enable_preempt)
  not t0, x0
  csrs CSR_SIE, t0
  jr ra
ENDPROC(enable_preempt)

ENTRY(disable_preempt)
  csrw CSR_SIE, zero
  jr ra
ENDPROC(disable_preempt)

ENTRY(enable_interrupt)
  li t0, SR_SIE
  csrs CSR_SSTATUS, t0
  jr ra
ENDPROC(enable_interrupt)

ENTRY(disable_interrupt)
  li t0, SR_SIE
  csrc CSR_SSTATUS, t0
  jr ra
ENDPROC(disable_interrupt)

// NOTE: the address of previous pcb in a0
// NOTE: the address of next pcb in a1
ENTRY(switch_to)
  /* save all callee save registers on kernel stack,
   * see the definition of `struct switchto_context` in sched.h
   */
  
  // load the address of kernel stack into a0
  // a0 <- prev->kernel_sp
  ld  a0, PCB_KERNEL_SP(a0)
  addi a0, a0, -(SWITCH_TO_SIZE)

  // push the values of callee-save registers to the kernel stack
  sd ra,  SWITCH_TO_RA (a0)
  sd sp,  SWITCH_TO_SP (a0)
  sd s0,  SWITCH_TO_S0 (a0)
  sd s1,  SWITCH_TO_S1 (a0)
  sd s2,  SWITCH_TO_S2 (a0)
  sd s3,  SWITCH_TO_S3 (a0)
  sd s4,  SWITCH_TO_S4 (a0)
  sd s5,  SWITCH_TO_S5 (a0)
  sd s6,  SWITCH_TO_S6 (a0)
  sd s7,  SWITCH_TO_S7 (a0)
  sd s8,  SWITCH_TO_S8 (a0)
  sd s9,  SWITCH_TO_S9 (a0)
  sd s10, SWITCH_TO_S10(a0)
  sd s11, SWITCH_TO_S11(a0)

  /* restore all callee save registers from kernel stack,
   * see the definition of `struct switchto_context` in sched.h
   */

  // load the address of kernel stack into a1
  // a1 <- next->kernel_sp
  ld a1,               (a1)
  addi a1, a1, -(SWITCH_TO_SIZE)

  // load the values of callee-save registers from stack
  ld ra,  SWITCH_TO_RA (a1)
  ld sp,  SWITCH_TO_SP (a1)
  ld s0,  SWITCH_TO_S0 (a1)
  ld s1,  SWITCH_TO_S1 (a1)
  ld s2,  SWITCH_TO_S2 (a1)
  ld s3,  SWITCH_TO_S3 (a1)
  ld s4,  SWITCH_TO_S4 (a1)
  ld s5,  SWITCH_TO_S5 (a1)
  ld s6,  SWITCH_TO_S6 (a1)
  ld s7,  SWITCH_TO_S7 (a1)
  ld s8,  SWITCH_TO_S8 (a1)
  ld s9,  SWITCH_TO_S9 (a1)
  ld s10, SWITCH_TO_S10(a1)
  ld s11, SWITCH_TO_S11(a1)

  jr ra
ENDPROC(switch_to)

ENTRY(ret_from_exception)
  /* restore context via provided macro and return to sepc */
  RESTORE_CONTEXT
  sret
ENDPROC(ret_from_exception)

ENTRY(exception_handler_entry)
  /* save context via the provided macro */
  SAVE_CONTEXT

  /* load ret_from_exception into $ra so that we can return to
   * ret_from_exception when interrupt_help complete.
   */
  la ra, ret_from_exception

  /* call interrupt_helper */
  mv a0, tp
  ld a0, PCB_KERNEL_SP(a0)
  csrr a1, stval
  csrr a2, scause

  call interrupt_helper
ENDPROC(exception_handler_entry)
