#!/usr/bin/env python3
"""
gtkwave_decode.py

A GTKWave "Translate Filter Process" that decodes riscvmove buses into
human-readable mnemonics -- point any RV32I instruction bus at this script
and see "addi a0,zero,5" instead of "00500513"; point alu_op at it and see
"ALU_ADD" instead of "0".

HOW TO USE IN GTKWAVE
----------------------
1. Open your .fst waveform and add the bus you want decoded (e.g. `instr`
   or `alu_op`) to the signal list.
2. Left-click the signal name to highlight it.
3. Edit -> Data Format -> Hex               (the bus MUST be in Hex format --
   this filter reads the same hex text GTKWave displays)
4. Edit -> Data Format -> Translate Filter Process -> Enable and Select
5. Add Proc Filter to List, then Browse to this file (make it executable
   first: chmod +x gtkwave_decode.py), select it, OK.
The trace will now show decoded mnemonics instead of raw hex. To turn it
off: Edit -> Data Format -> Translate Filter Process -> Disable.

WHY ONE SCRIPT CAN HANDLE BOTH BUSES
--------------------------------------
GTKWave's process-filter protocol (see its own manual, "Alias Files and
Attaching External Disassemblers") is deliberately minimal: for every
value change on a highlighted trace, GTKWave writes one hex string to this
process's stdin -- zero-padded to that trace's *own* bit width -- and reads
back one translated line from stdout. It never tells the filter which
signal or how many bits wide the source is beyond that implicit padding,
and if several differently-named traces share the filter, they're all fed
into the same stdin stream with no tag distinguishing them. So this script
identifies what it's looking at by the padded string length alone:
  - 8 hex digits (32 bits)      -> decode as an RV32I instruction word
  - 1-2 hex digits, value 0-10  -> decode as this project's alu_op encoding
    (src/riscvmove_defs.sv's alu_op_t enum -- NOT part of the RV32I ISA,
    it's this CPU's own internal ALU control code)
  - anything else               -> passed through unchanged (raw:<hex>),
    since guessing would just be wrong for a bus this script wasn't meant
    to see. This also means: don't apply this filter to some OTHER 32-bit
    bus (monitor_pc, mmio_addr, rd1, ...) and expect a meaningful answer --
    the protocol gives this script no way to tell those apart from instr,
    so it will cheerfully "decode" any 32-bit value as if it were an
    instruction. Point it only at an actual instruction bus or alu_op.

Different testbenches/top-levels may name their instruction bus and ALU
opcode signal differently (instr vs instruction vs id_instr, alu_op vs
aluop vs alu_ctrl, etc.) -- that's fine, this script never looks at the
signal name (GTKWave never sends it), only at the value's width, so it
works unmodified regardless of what a given student calls the wire.

Register operands are printed with their RISC-V ABI names (zero, ra, sp,
a0, t0, ...) -- the same convention `riscv64-unknown-elf-objdump -d` uses,
so a decoded trace lines up directly with `objdump -d build/program.elf`.

Standalone testing (no GTKWave needed):
    echo 00500513 | python3 gtkwave_decode.py     # -> addi a0,zero,5
    echo 0         | python3 gtkwave_decode.py     # -> ALU_ADD
"""

import sys

REG_ABI = [
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6",
]

# src/riscvmove_defs.sv: alu_op_t. This is riscvmove's own internal ALU
# control encoding, not a RISC-V standard -- keep in sync with that file.
ALU_OPS = {
    0: "ALU_ADD", 1: "ALU_SUB", 2: "ALU_SLL", 3: "ALU_SLT",
    4: "ALU_SLTU", 5: "ALU_XOR", 6: "ALU_SRL", 7: "ALU_SRA",
    8: "ALU_OR", 9: "ALU_AND", 10: "ALU_COPY_B",
}

OP_LOAD, OP_STORE, OP_OP_IMM, OP_OP = 0x03, 0x23, 0x13, 0x33
OP_BRANCH, OP_JALR, OP_JAL = 0x63, 0x67, 0x6F
OP_LUI, OP_AUIPC, OP_SYSTEM = 0x37, 0x17, 0x73


def reg(n):
    return REG_ABI[n & 0x1F]


def sext(value, bits):
    sign_bit = 1 << (bits - 1)
    return (value & (sign_bit - 1)) - (value & sign_bit)


def signed_str(imm):
    return f"{imm}" if imm < 0 else f"+{imm}" if imm > 0 else "0"


def decode_instruction(word):
    if word & 0x3 != 0x3:
        return f"illegal({word:08x})"  # not a valid 32-bit RV32I encoding

    opcode = word & 0x7F
    rd = (word >> 7) & 0x1F
    funct3 = (word >> 12) & 0x7
    rs1 = (word >> 15) & 0x1F
    rs2 = (word >> 20) & 0x1F
    funct7 = (word >> 25) & 0x7F

    imm_i = sext(word >> 20, 12)
    imm_s = sext(((word >> 25) << 5) | ((word >> 7) & 0x1F), 12)
    imm_b = sext(((word >> 31) << 12) | (((word >> 7) & 0x1) << 11) |
                 (((word >> 25) & 0x3F) << 5) | (((word >> 8) & 0xF) << 1), 13)
    imm_u = word & 0xFFFFF000
    imm_j = sext(((word >> 31) << 20) | (((word >> 12) & 0xFF) << 12) |
                 (((word >> 20) & 0x1) << 11) | (((word >> 21) & 0x3FF) << 1), 21)

    if word == 0x00000013:
        return "nop"

    if opcode == OP_OP:
        name = {0: "sub" if funct7 & 0x20 else "add", 1: "sll", 2: "slt",
                3: "sltu", 4: "xor", 5: "sra" if funct7 & 0x20 else "srl",
                6: "or", 7: "and"}.get(funct3, f"op?{funct3}")
        return f"{name} {reg(rd)},{reg(rs1)},{reg(rs2)}"

    if opcode == OP_OP_IMM:
        if funct3 == 0:
            if rs1 == 0:
                return f"li {reg(rd)},{imm_i}"
            if imm_i == 0:
                return f"mv {reg(rd)},{reg(rs1)}"
            return f"addi {reg(rd)},{reg(rs1)},{imm_i}"
        if funct3 == 1:
            return f"slli {reg(rd)},{reg(rs1)},{word >> 20 & 0x1F}"
        if funct3 == 5:
            shamt = word >> 20 & 0x1F
            return (f"srai {reg(rd)},{reg(rs1)},{shamt}" if funct7 & 0x20
                    else f"srli {reg(rd)},{reg(rs1)},{shamt}")
        if funct3 == 4 and imm_i == -1:
            return f"not {reg(rd)},{reg(rs1)}"
        name = {2: "slti", 3: "sltiu", 4: "xori", 6: "ori", 7: "andi"}.get(funct3, f"op-imm?{funct3}")
        return f"{name} {reg(rd)},{reg(rs1)},{imm_i}"

    if opcode == OP_LOAD:
        name = {0: "lb", 1: "lh", 2: "lw", 4: "lbu", 5: "lhu"}.get(funct3, f"load?{funct3}")
        return f"{name} {reg(rd)},{imm_i}({reg(rs1)})"

    if opcode == OP_STORE:
        name = {0: "sb", 1: "sh", 2: "sw"}.get(funct3, f"store?{funct3}")
        return f"{name} {reg(rs2)},{imm_s}({reg(rs1)})"

    if opcode == OP_BRANCH:
        name = {0: "beq", 1: "bne", 4: "blt", 5: "bge", 6: "bltu", 7: "bgeu"}.get(funct3, f"branch?{funct3}")
        if funct3 == 0 and rs2 == 0:
            return f"beqz {reg(rs1)},{signed_str(imm_b)}"
        if funct3 == 1 and rs2 == 0:
            return f"bnez {reg(rs1)},{signed_str(imm_b)}"
        return f"{name} {reg(rs1)},{reg(rs2)},{signed_str(imm_b)}"

    if opcode == OP_JAL:
        if rd == 0:
            return f"j {signed_str(imm_j)}"
        return f"jal {reg(rd)},{signed_str(imm_j)}"

    if opcode == OP_JALR:
        if rd == 0 and rs1 == 1 and imm_i == 0:
            return "ret"
        if rd == 0 and imm_i == 0:
            return f"jr {reg(rs1)}"
        return f"jalr {reg(rd)},{imm_i}({reg(rs1)})"

    if opcode == OP_LUI:
        return f"lui {reg(rd)},0x{imm_u >> 12:x}"

    if opcode == OP_AUIPC:
        return f"auipc {reg(rd)},0x{imm_u >> 12:x}"

    if opcode == OP_SYSTEM:
        imm12 = word >> 20 & 0xFFF
        if imm12 == 0:
            return "ecall"
        if imm12 == 1:
            return "ebreak"
        return f"system({imm12:03x})"

    return f"unknown-opcode({opcode:02x})"


def decode_alu_op(value):
    return ALU_OPS.get(value, f"alu_op?{value}")


def translate(token):
    token = token.strip()
    if not token:
        return ""
    # GTKWave uses x/z/u/w/- for undefined/high-impedance/uninitialized
    # bits; a translate filter must never crash on these (it would kill the
    # persistent process and hang the viewer), so hand them back verbatim.
    if any(c not in "0123456789abcdefABCDEF" for c in token):
        return token
    try:
        value = int(token, 16)
    except ValueError:
        return token

    if len(token) > 2:
        return decode_instruction(value)
    if 0 <= value <= 10:
        return decode_alu_op(value)
    return f"raw:{token}"


def main():
    for line in sys.stdin:
        try:
            result = translate(line)
        except Exception as exc:  # noqa: BLE001 -- must never die mid-stream
            result = f"decode-error:{exc}"
        sys.stdout.write(result + "\n")
        sys.stdout.flush()


if __name__ == "__main__":
    main()
