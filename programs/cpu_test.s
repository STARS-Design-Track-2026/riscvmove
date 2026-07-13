# cpu_test.s
# Canonical regression program for verify_riscvmove / tb_riscvmove.cpp.
# Exercises immediate loads, back-to-back register dependencies, a taken
# branch (with a fall-through instruction that must never execute), a
# same-cycle read+write of one register (regfile.sv's no-bypass path), a
# store/load round trip through dmem, an auipc, and a jal/jalr call+return
# pair -- then lands 30 in x4 and halts via ecall. Comfortably within the
# 64-instruction target (17 instructions).

.section .text.init
.global _start

_start:
    li   x5, 10
    li   x6, 20
    add  x4, x5, x6      # x4 = 30 (forwarded to the next two instructions)
    add  x7, x4, x0      # reuse of x4's freshly-written value
    beq  x4, x4, target  # always taken
    addi x4, x4, 1000    # must never execute

target:
    li   x9, 5
    addi x9, x9, 1       # same-cycle read+write of x9: x9 = 6

    sw   x4, 0(x0)       # store 30 to dmem[0]
    lw   x10, 0(x0)      # load it back: x10 = 30

    auipc x11, 0         # x11 = this instruction's own pc

    jal  x1, callee      # call
    j    after_call

callee:
    li   x12, 42
    jalr x0, 0(x1)       # return

after_call:
    ecall

spin:
    j spin
