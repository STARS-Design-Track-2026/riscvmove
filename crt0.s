# crt0.s
# Startup stub: initialize the stack pointer, zero .bss, then call main.
#
# .data needs no copy here -- dmem.sv is preloaded with .data's initial
# values directly (there's no lw/sw path from dmem back into imem's BRAM to
# copy them from at runtime; see link.ld and the Makefile).

.section .text.init
.global _start

_start:
    la   sp, _dmem_top

    # Zero .bss
    la   t1, _bss_start
    la   t2, _bss_end
zero_bss:
    bge  t1, t2, zero_bss_done
    sw   x0, 0(t1)
    addi t1, t1, 4
    j    zero_bss
zero_bss_done:

    # Call main
    call main

    # Exit program by triggering ECALL (halting CPU in simulation)
    ecall

# Spin loop in case ecall returns or is not handled
spin:
    j spin
