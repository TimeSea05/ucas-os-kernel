#include <asm/biosdef.h>
#include <csr.h>

#define KERNEL_SECTOR_ID 1
#define READ_BLOCK_MAX_NUM 64
// os size location
.equ os_size_loc, 0x502001fc

// kernel address (move kernel to here, and jmp here to start kernel)
.equ kernel, 0x50202000

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000

.text
.global main

main:
	// fence on all memory and I/O
	fence

	// a0 != 0 => slave processor
	// go to secondary
	bnez a0, secondary

	// a0 is mhartid
	// save mhartid to sscratch
	csrw sscratch, a0

	// call bios_read to load kernel into memory
	// The maximum number of blocks read at one time is 64
	li a0, BIOS_SDREAD
	li a1, kernel
	li t3, kernel 

	li t0, os_size_loc
	ld t1, (t0)
	li t2, 0xffff								# mask: in createimage, typeof nkernel_sectors: short
	and t1, t1, t2		  				# set t1 to nkernel_sectors
	li t2, READ_BLOCK_MAX_NUM		# set t2 to READ_BLOCK_MAX_NUM

	mv a2, zero
	li a3, KERNEL_SECTOR_ID
	ble t1, t2, loop_end

	loop:
		li a2, READ_BLOCK_MAX_NUM
		li t4, 32768
		add t3, t3, t4           # 64 sectors * 512 bytes
		call bios_func_entry

		li a0, BIOS_SDREAD
		mv a1, t3

		mv a3, a2
		addi a3, a3, READ_BLOCK_MAX_NUM

		addi t1, t1, -READ_BLOCK_MAX_NUM
		bge  t1, t2, loop

	loop_end:
		mv a2, t1
		call bios_func_entry

	csrr a0, sscratch					# load mhartid from sscratch
	call kernel

secondary:
	// set SSTATUS.SIE to 1
	csrr t0, sstatus
	li t1, SR_SIE
	or t0, t0, t1
	csrw sstatus, t0

	// mask external interrupt, timer interrupt
	csrr t0, sie
	li t1, SIE_STIE
	ori t1, t1, SIE_SEIE
	not t1, t1
	and t0, t0, t1

	// enable software interrupt for ipi
	ori t0, t0, SIE_SSIE
	csrw sie, t0

	// set stvec pointer of slave processor to kernel.start(head.S)
	la t0, kernel
	csrw stvec, t0

	// set stack pointer for cpu1

	j wait_for_wakeup

wait_for_wakeup:
	wfi
	j wait_for_wakeup

.data

msg: .string "It's a bootloader...\n\r"

