// tb_imm_gen.cpp
// Verilator testbench for the immediate builder (imm_gen)
//
// Rather than hand-computing expected hex encodings, each RV32I immediate
// format gets its own encode_*() helper that packs a *known* signed value
// into the real scattered bit layout that format uses (the exact inverse of
// imm_gen.sv's own reconstruction formula). Feeding the packed instruction
// through imm_gen and asserting the result equals the original value tests
// reconstruction directly, independent of arithmetic done by hand. Every
// non-immediate field (rs1/rs2/rd/funct3 where applicable) is packed with
// all-ones "noise" instead of zero, so a bug that accidentally leaks a
// neighboring field's bits into imm would show up as a wrong result instead
// of being masked by coincidentally-zero inputs.

#include "Vimm_gen.h"
#include "verilated.h"
#include "verilated_fst_c.h"
#include <iostream>
#include <cassert>
#include <cstdint>

namespace {

uint32_t encode_i(int32_t imm, uint32_t opcode) {
    uint32_t imm12 = (uint32_t)imm & 0xFFF;
    return (imm12 << 20) | (0x1Fu << 15) | (0x7u << 12) | (0x1Fu << 7) | (opcode & 0x7F);
}

uint32_t encode_s(int32_t imm, uint32_t opcode) {
    uint32_t imm12  = (uint32_t)imm & 0xFFF;
    uint32_t imm_hi = (imm12 >> 5) & 0x7F; // -> instr[31:25]
    uint32_t imm_lo = imm12 & 0x1F;        // -> instr[11:7]
    return (imm_hi << 25) | (0x1Fu << 20) | (0x1Fu << 15) | (0x7u << 12) | (imm_lo << 7) | (opcode & 0x7F);
}

uint32_t encode_b(int32_t imm, uint32_t opcode) {
    uint32_t v     = (uint32_t)imm;
    uint32_t b12   = (v >> 12) & 0x1;
    uint32_t b11   = (v >> 11) & 0x1;
    uint32_t b10_5 = (v >> 5)  & 0x3F;
    uint32_t b4_1  = (v >> 1)  & 0xF;
    return (b12 << 31) | (b10_5 << 25) | (0x1Fu << 20) | (0x1Fu << 15) |
           (0x7u << 12) | (b4_1 << 8) | (b11 << 7) | (opcode & 0x7F);
}

uint32_t encode_u(int32_t imm, uint32_t opcode) {
    // imm is already the full 32-bit value with its low 12 bits zero,
    // matching what imm_gen itself produces for U-type.
    uint32_t v = (uint32_t)imm;
    return (v & 0xFFFFF000u) | (0x1Fu << 7) | (opcode & 0x7F);
}

uint32_t encode_j(int32_t imm, uint32_t opcode) {
    uint32_t v      = (uint32_t)imm;
    uint32_t b20    = (v >> 20) & 0x1;
    uint32_t b19_12 = (v >> 12) & 0xFF;
    uint32_t b11    = (v >> 11) & 0x1;
    uint32_t b10_1  = (v >> 1)  & 0x3FF;
    return (b20 << 31) | (b10_1 << 21) | (b11 << 20) | (b19_12 << 12) | (0x1Fu << 7) | (opcode & 0x7F);
}

} // namespace

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);
    Vimm_gen* duv = new Vimm_gen;

    VerilatedFstC* tfp = new VerilatedFstC;
    duv->trace(tfp, 99);
    tfp->open("waves/imm_gen.fst");
    uint64_t sim_time = 0;

    auto check = [&](const char* name, uint32_t instr, int32_t expected) {
        duv->instr = instr;
        duv->eval();
        tfp->dump(sim_time);
        sim_time++;
        std::cout << name << ": instr=0x" << std::hex << instr
                   << " imm=0x" << (uint32_t)duv->imm << std::dec
                   << " (" << (int32_t)duv->imm << ")" << std::endl;
        assert((int32_t)duv->imm == expected);
    };

    // I-type: OP_IMM, JALR, and LOAD all share this same layout.
    check("I-type positive (OP_IMM)", encode_i(10, 0b0010011), 10);
    check("I-type negative (OP_IMM)", encode_i(-1, 0b0010011), -1);
    check("I-type negative (JALR)",   encode_i(-100, 0b1100111), -100);
    check("I-type max positive (LOAD)", encode_i(2047, 0b0000011), 2047);
    check("I-type min negative (OP_IMM)", encode_i(-2048, 0b0010011), -2048);

    // S-type: STORE.
    check("S-type positive (STORE)", encode_s(100, 0b0100011), 100);
    check("S-type negative (STORE)", encode_s(-1, 0b0100011), -1);
    check("S-type min negative (STORE)", encode_s(-2048, 0b0100011), -2048);
    check("S-type max positive (STORE)", encode_s(2047, 0b0100011), 2047);

    // B-type: BRANCH. Offsets are always even (bit 0 is never encoded).
    check("B-type positive (BRANCH)", encode_b(100, 0b1100011), 100);
    check("B-type negative (BRANCH)", encode_b(-4, 0b1100011), -4);
    check("B-type max positive (BRANCH)", encode_b(4094, 0b1100011), 4094);
    check("B-type min negative (BRANCH)", encode_b(-4096, 0b1100011), -4096);

    // U-type: LUI / AUIPC. Same case arm in imm_gen for both opcodes.
    check("U-type (LUI)",   encode_u((int32_t)0x80000000, 0b0110111), (int32_t)0x80000000);
    check("U-type (AUIPC)", encode_u(0x12345000, 0b0010111), 0x12345000);
    check("U-type zero (LUI)", encode_u(0, 0b0110111), 0);

    // J-type: JAL. Offsets are always even (bit 0 is never encoded).
    check("J-type positive (JAL)", encode_j(2046, 0b1101111), 2046);
    check("J-type negative (JAL)", encode_j(-2, 0b1101111), -2);
    check("J-type max positive (JAL)", encode_j(1048574, 0b1101111), 1048574);
    check("J-type min negative (JAL)", encode_j(-1048576, 0b1101111), -1048576);

    // Unallocated-for-this-module opcode: OP (register-register ALU ops)
    // carries no immediate at all, so imm_gen must default to 0. Clears all
    // 7 opcode bits before ORing in 0110011 -- a narrower mask here would
    // silently leave garbage in bits[6:5] and test some other opcode
    // instead (e.g. OP_SYSTEM), even though both would coincidentally pass
    // the same assertion.
    check("OP (no immediate)", (0xFFFFFFFFu & ~0x7Fu) | 0b0110011u, 0);

    tfp->close();
    delete tfp;
    delete duv;
    std::cout << "IMM_GEN TEST PASSED!" << std::endl;
    return 0;
}
