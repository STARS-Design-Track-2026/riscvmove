// tb_imem.cpp
// Verilator testbench for the instruction ROM (negedge-clocked read).
// Built against a small 4-word fixture (test/imem_fixture.hex, DEPTH=4 --
// see the Makefile's verify_imem target) so each word is distinguishable.

#include "Vimem.h"
#include "verilated.h"
#include <iostream>
#include <cassert>

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vimem* duv = new Vimem;

    duv->memclk = 0;
    duv->addr_a = 0;
    duv->eval();
    assert(duv->dout_a == 0); // nothing captured yet

    // A rising edge alone must not update dout_a -- this module reads on
    // the falling edge specifically (see imem.sv for why).
    duv->addr_a = 0;
    duv->memclk = 1;
    duv->eval();
    assert(duv->dout_a == 0);
    std::cout << "Rising edge alone leaves dout_a unchanged\n";

    // The following falling edge captures mem[addr_a].
    duv->memclk = 0;
    duv->eval();
    std::cout << "Falling edge: dout_a = 0x" << std::hex << duv->dout_a << std::dec << std::endl;
    assert(duv->dout_a == 0xdeadbeef);

    // Change address mid-cycle (as riscvmove.sv does when pc updates on a
    // rising edge). Confirm the new word is captured on the *next* falling
    // edge, not immediately and not on the intervening rising edge.
    duv->addr_a = 4;
    duv->eval();
    assert(duv->dout_a == 0xdeadbeef); // still the old word, no edge yet
    duv->memclk = 1;
    duv->eval();
    assert(duv->dout_a == 0xdeadbeef); // rising edge doesn't act
    duv->memclk = 0;
    duv->eval();
    std::cout << "Read word 1: 0x" << std::hex << duv->dout_a << std::dec << std::endl;
    assert(duv->dout_a == 0xcafebabe);

    duv->addr_a = 12;
    duv->memclk = 1;
    duv->eval();
    duv->memclk = 0;
    duv->eval();
    std::cout << "Read word 3: 0x" << std::hex << duv->dout_a << std::dec << std::endl;
    assert(duv->dout_a == 0x12345678);

    delete duv;
    std::cout << "IMEM TEST PASSED!" << std::endl;
    return 0;
}
