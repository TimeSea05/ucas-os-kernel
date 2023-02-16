#include <asm.h>

.section ".entry_function","ax"
ENTRY(_start)
    la t0, __bss_start
    la t1, __BSS_END__

    clearbss:
        bge t0, t1, clearbss_exit
        sd zero, (t0)
        addi t0, t0, 4
        j clearbss

    clearbss_exit:

    call main

    /* call sys_exit */
    call sys_exit

END(_start)
