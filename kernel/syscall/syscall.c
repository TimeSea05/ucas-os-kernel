#include <sys/syscall.h>

#define REG_POS_A0      10    /* Index of a0 register in regs_context_t->regs */
#define REG_POS_A1      11    /* Index of a1 register in regs_context_t->regs */
#define REG_POS_A2      12    /* Index of a2 register in regs_context_t->regs */
#define REG_POS_A3      13    /* Index of a3 register in regs_context_t->regs */
#define REG_POS_A4      14    /* Index of a4 register in regs_context_t->regs */
#define REG_POS_A5      15    /* Index of a5 register in regs_context_t->regs */
#define REG_POS_A7      17    /* Index of a7 register in regs_context_t->regs */

long (*syscall[NUM_SYSCALLS])();

void handle_syscall(regs_context_t *regs, uint64_t interrupt, uint64_t cause)
{
    long syscall_number = regs->regs[REG_POS_A7];
    
    long arg0 = regs->regs[REG_POS_A0];
    long arg1 = regs->regs[REG_POS_A1];
    long arg2 = regs->regs[REG_POS_A2];
    long arg3 = regs->regs[REG_POS_A3];
    long arg4 = regs->regs[REG_POS_A4];

    regs->regs[REG_POS_A0] = syscall[syscall_number](arg0, arg1, arg2, arg3, arg4);
    regs->sepc += 4;
}
