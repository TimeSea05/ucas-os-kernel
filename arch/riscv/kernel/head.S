/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK_VA_0		 0xffffffc050600000
#define KERNEL_STACK_VA_1    0xffffffc050700000

ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  // only master processor can clean bss
  // which means a0 == 0
  bnez a0, clearbss_exit

  la t0, __bss_start
  la t1, __BSS_END__

  clearbss:
    bge t0, t1, clearbss_exit
    sd zero, 0(t0)
    addi t0, t0, 4
    j clearbss

  clearbss_exit:
    li sp, KERNEL_STACK_VA_0
    
    // set sp to KERNEL_STACK_VA_1
    // if a0 == 1(slave processor)
    mv a1, a0
    slli a1, a1, 20
    or sp, sp, a1;

    // set sstatus.SUM to 1
    csrr t0, sstatus
    li t1, SR_SUM
    or t0, t0, t1
    csrw sstatus, t0

    call main

loop:
  wfi
  j loop

END(_start)
