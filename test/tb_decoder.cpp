// tb_decoder.cpp
// Verilator testbench for the instruction decoder / control unit (decoder)
//
// Exercises every case arm in both of decoder.sv's always_comb blocks: the
// main decoder (one arm per opcode) and the ALU control decoder (one arm
// per opcode, further split by funct3/funct7 for OP_IMM and OP). Every
// opcode RV32I actually defines is covered, plus one unallocated opcode to
// confirm the safe all-zero fallback.

#include "Vdecoder.h"
#include "verilated.h"
#include "verilated_fst_c.h"
#include <cstdint>
#include <iostream>
#include <cassert>

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);
    Vdecoder* duv = new Vdecoder;

    VerilatedFstC* tfp = new VerilatedFstC;
    duv->trace(tfp, 99);
    tfp->open("waves/decoder.fst");
    uint64_t sim_time = 0;

    auto set = [&](uint32_t opcode, uint32_t funct3, uint32_t funct7) {
        duv->opcode = opcode & 0x7F;
        duv->funct3 = funct3 & 0x7;
        duv->funct7 = funct7 & 0x7F;
        duv->eval();
        tfp->dump(sim_time);
        sim_time++;
    };

    // Checks every control output at once. Field order matches decoder.sv's
    // port list so a failing assert's line number plus this signature is
    // enough to tell which signal was wrong.
    auto expect = [&](const char* name, int alu_op, int regwrite, int alu_src_b,
                       int memtoreg, int memwrite, int branch, int jal, int jalr,
                       int lui, int auipc) {
        std::cout << name
                   << ": alu_op="    << (int)duv->alu_op
                   << " regwrite="   << (int)duv->regwrite
                   << " alu_src_b="  << (int)duv->alu_src_b
                   << " memtoreg="   << (int)duv->memtoreg
                   << " memwrite="   << (int)duv->memwrite
                   << " branch="     << (int)duv->branch
                   << " jal="        << (int)duv->jal
                   << " jalr="       << (int)duv->jalr
                   << " lui="        << (int)duv->lui
                   << " auipc="      << (int)duv->auipc
                   << std::endl;
        assert((int)duv->alu_op   == alu_op);
        assert((int)duv->regwrite == regwrite);
        assert((int)duv->alu_src_b == alu_src_b);
        assert((int)duv->memtoreg == memtoreg);
        assert((int)duv->memwrite == memwrite);
        assert((int)duv->branch   == branch);
        assert((int)duv->jal      == jal);
        assert((int)duv->jalr     == jalr);
        assert((int)duv->lui      == lui);
        assert((int)duv->auipc    == auipc);
    };

    // LUI: regwrite + alu_src_b + lui; ALU passes the immediate straight
    // through unmodified (COPY_B = 10).
    set(0b0110111, 0, 0);
    expect("LUI",   /*alu_op*/10, /*rw*/1, /*asb*/1, /*mtr*/0, /*mw*/0, /*br*/0, /*jal*/0, /*jalr*/0, /*lui*/1, /*auipc*/0);

    // AUIPC: regwrite + alu_src_b + auipc; not one of the ALU control's
    // explicit cases, so it falls to the default ADD (pc is wired in as
    // SrcA outside this module -- see riscvmove.sv).
    set(0b0010111, 0, 0);
    expect("AUIPC", 0, 1, 1, 0, 0, 0, 0, 0, 0, 1);

    // JAL: regwrite + jal only; SrcB doesn't matter (no ALU use), ALU
    // control defaults to ADD.
    set(0b1101111, 0, 0);
    expect("JAL", 0, 1, 0, 0, 0, 0, 1, 0, 0, 0);

    // JALR: regwrite + alu_src_b + jalr; ALU defaults to ADD, computing
    // rs1+imm for free (see riscvmove.sv's next-PC mux, which reuses
    // alu_result directly as the jump target).
    set(0b1100111, 0, 0);
    expect("JALR", 0, 1, 1, 0, 0, 0, 0, 1, 0, 0);

    // BRANCH: branch only, regardless of funct3 -- the actual
    // BEQ/BNE/BLT/... distinction happens outside this module, off funct3
    // directly, in riscvmove.sv's branch_taken logic. ALU control forces
    // SUB (1) so zero/less/lessu resolve the comparison.
    set(0b1100011, 0b000, 0);
    expect("BRANCH (funct3=000)", 1, 0, 0, 0, 0, 1, 0, 0, 0, 0);
    set(0b1100011, 0b101, 0x7F);
    expect("BRANCH (funct3=101, funct7 varied)", 1, 0, 0, 0, 0, 1, 0, 0, 0, 0);

    // LOAD: regwrite + alu_src_b + memtoreg; ALU defaults to ADD
    // (address = rs1+imm).
    set(0b0000011, 0, 0);
    expect("LOAD", 0, 1, 1, 1, 0, 0, 0, 0, 0, 0);

    // STORE: alu_src_b + memwrite only (no regwrite); ALU defaults to ADD.
    set(0b0100011, 0, 0);
    expect("STORE", 0, 0, 1, 0, 1, 0, 0, 0, 0, 0);

    // OP_IMM: regwrite + alu_src_b always; alu_op swept across every
    // funct3 (and funct7[5] for the SRLI/SRAI split).
    struct AluCase { uint32_t funct3; uint32_t funct7; int alu_op; const char* name; };
    const AluCase op_imm_cases[] = {
        {0b000, 0x00, 0,  "OP_IMM ADDI"},
        {0b010, 0x00, 3,  "OP_IMM SLTI"},
        {0b011, 0x00, 4,  "OP_IMM SLTIU"},
        {0b100, 0x00, 5,  "OP_IMM XORI"},
        {0b110, 0x00, 8,  "OP_IMM ORI"},
        {0b111, 0x00, 9,  "OP_IMM ANDI"},
        {0b001, 0x00, 2,  "OP_IMM SLLI"},
        {0b101, 0x00, 6,  "OP_IMM SRLI (funct7[5]=0)"},
        {0b101, 0x20, 7,  "OP_IMM SRAI (funct7[5]=1)"},
    };
    for (const auto& c : op_imm_cases) {
        set(0b0010011, c.funct3, c.funct7);
        expect(c.name, c.alu_op, 1, 1, 0, 0, 0, 0, 0, 0, 0);
    }

    // OP: regwrite only (no alu_src_b); alu_op swept the same way, plus the
    // ADD/SUB split on funct7[5] for funct3=000.
    const AluCase op_cases[] = {
        {0b000, 0x00, 0,  "OP ADD (funct7[5]=0)"},
        {0b000, 0x20, 1,  "OP SUB (funct7[5]=1)"},
        {0b001, 0x00, 2,  "OP SLL"},
        {0b010, 0x00, 3,  "OP SLT"},
        {0b011, 0x00, 4,  "OP SLTU"},
        {0b100, 0x00, 5,  "OP XOR"},
        {0b101, 0x00, 6,  "OP SRL (funct7[5]=0)"},
        {0b101, 0x20, 7,  "OP SRA (funct7[5]=1)"},
        {0b110, 0x00, 8,  "OP OR"},
        {0b111, 0x00, 9,  "OP AND"},
    };
    for (const auto& c : op_cases) {
        set(0b0110011, c.funct3, c.funct7);
        expect(c.name, c.alu_op, 1, 0, 0, 0, 0, 0, 0, 0, 0);
    }

    // Unallocated opcode: every output must fall back to its safe default
    // (no register write, no memory write, pc just advances via pc+4
    // outside this module) -- confirms the "unrecognized opcode is a
    // harmless no-op" behavior documented in SUBMODULES.md.
    set(0b1111111, 0b111, 0x7F);
    expect("Unallocated opcode", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

    tfp->close();
    delete tfp;
    delete duv;
    std::cout << "DECODER TEST PASSED!" << std::endl;
    return 0;
}
