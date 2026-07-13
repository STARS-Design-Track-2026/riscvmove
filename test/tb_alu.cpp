// tb_alu.cpp
// Verilator testbench for ALU

#include "Valu.h"
#include "verilated.h"
#include <iostream>
#include <cassert>

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Valu* duv = new Valu;

    // Test ADD: 10 + 20 = 30
    duv->op = 0; // ALU_ADD = 0
    duv->a = 10;
    duv->b = 20;
    duv->eval();
    std::cout << "ADD test: 10 + 20 = " << duv->result << std::endl;
    assert(duv->result == 30);

    // Test SUB: 20 - 5 = 15
    duv->op = 1; // ALU_SUB = 1
    duv->a = 20;
    duv->b = 5;
    duv->eval();
    std::cout << "SUB test: 20 - 5 = " << duv->result << std::endl;
    assert(duv->result == 15);

    // Test SLL: 1 << 4 = 16
    duv->op = 2; // ALU_SLL = 2
    duv->a = 1;
    duv->b = 4;
    duv->eval();
    std::cout << "SLL test: 1 << 4 = " << duv->result << std::endl;
    assert(duv->result == 16);

    // Test SLT: 10 < 20 = 1 (signed)
    duv->op = 3; // ALU_SLT = 3
    duv->a = 10;
    duv->b = 20;
    duv->eval();
    std::cout << "SLT test: 10 < 20 = " << duv->result << std::endl;
    assert(duv->result == 1);

    // Test SLT: -5 < 2 = 1 (signed)
    duv->a = -5;
    duv->b = 2;
    duv->eval();
    std::cout << "SLT signed test: -5 < 2 = " << duv->result << std::endl;
    assert(duv->result == 1);

    // Test SLTU: -5 < 2 = 0 (unsigned: -5 is 0xFFFFFFFB)
    duv->op = 4; // ALU_SLTU = 4
    duv->a = -5;
    duv->b = 2;
    duv->eval();
    std::cout << "SLTU unsigned test: -5 < 2 = " << duv->result << std::endl;
    assert(duv->result == 0);

    // Test XOR: 5 ^ 3 = 6
    duv->op = 5; // ALU_XOR = 5
    duv->a = 5;
    duv->b = 3;
    duv->eval();
    std::cout << "XOR test: 5 ^ 3 = " << duv->result << std::endl;
    assert(duv->result == 6);

    // Test SRL: 16 >> 2 = 4
    duv->op = 6; // ALU_SRL = 6
    duv->a = 16;
    duv->b = 2;
    duv->eval();
    std::cout << "SRL test: 16 >> 2 = " << duv->result << std::endl;
    assert(duv->result == 4);

    // Test SRA: -16 >>> 2 = -4
    duv->op = 7; // ALU_SRA = 7
    duv->a = -16;
    duv->b = 2;
    duv->eval();
    std::cout << "SRA test: -16 >>> 2 = " << (int)duv->result << std::endl;
    assert((int)duv->result == -4);

    // Test OR: 5 | 3 = 7
    duv->op = 8; // ALU_OR = 8
    duv->a = 5;
    duv->b = 3;
    duv->eval();
    std::cout << "OR test: 5 | 3 = " << duv->result << std::endl;
    assert(duv->result == 7);

    // Test AND: 5 & 3 = 1
    duv->op = 9; // ALU_AND = 9
    duv->a = 5;
    duv->b = 3;
    duv->eval();
    std::cout << "AND test: 5 & 3 = " << duv->result << std::endl;
    assert(duv->result == 1);

    delete duv;
    std::cout << "ALU TEST PASSED!" << std::endl;
    return 0;
}
