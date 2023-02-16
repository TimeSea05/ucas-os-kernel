#include <asm.h>
#include <csr.h>

ENTRY(setup_exception)

  /* save exception_handler_entry into STVEC */
  la t0, exception_handler_entry
  csrw stvec, t0

  /* enable interrupts globally */
  csrr t0, sie
  ori t0, t0, SIE_STIE    /* enable timer interrupt */
  csrw sie, t0

  ret
ENDPROC(setup_exception)
