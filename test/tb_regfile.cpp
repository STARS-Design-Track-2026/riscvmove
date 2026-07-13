// tb_regfile.cpp
// Verilator testbench for Register File

#include "Vregfile.h"
#include "verilated.h"
#include <iostream>
#include <cassert>

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vregfile* duv = new Vregfile;

    // Helper lambda to toggle clock
    auto tick = [&]() {
        duv->clk = 0;
        duv->eval();
        duv->clk = 1;
        duv->eval();
    };

    // Initialize inputs
    duv->we3 = 0;
    duv->ra1 = 0;
    duv->ra2 = 0;
    duv->wa3 = 0;
    duv->wd3 = 0;
    duv->eval();

    // Test x0 is always 0
    duv->ra1 = 0;
    duv->eval();
    std::cout << "Read x0: " << duv->rd1 << std::endl;
    assert(duv->rd1 == 0);

    // Write to x1
    duv->we3 = 1;
    duv->wa3 = 1;
    duv->wd3 = 0xDEADBEEF;
    tick(); // Posited edge updates rf[1]

    // Read x1
    duv->we3 = 0;
    duv->ra1 = 1;
    duv->eval();
    std::cout << "Read x1 (normal): 0x" << std::hex << duv->rd1 << std::dec << std::endl;
    assert(duv->rd1 == 0xDEADBEEF);

    // Test no bypass: writing to x2 and reading from x2 in the same cycle
    // must see the OLD stored value (0, since x2 was never written before),
    // not the pending write data -- single-cycle riscvmove relies on this
    // for instructions like `addi x5,x5,1` that read and write the same
    // register in one cycle (see regfile.sv for why a bypass would instead
    // create a combinational loop there).
    duv->we3 = 1;
    duv->wa3 = 2;
    duv->wd3 = 0xCAFEBABE;
    duv->ra1 = 2; // read same cycle
    duv->eval();
    std::cout << "Read x2 (same cycle as write, no bypass): 0x" << std::hex << duv->rd1 << std::dec << std::endl;
    assert(duv->rd1 == 0);

    tick(); // actually write it
    duv->we3 = 0;
    duv->eval();
    std::cout << "Read x2 (after clock): 0x" << std::hex << duv->rd1 << std::dec << std::endl;
    assert(duv->rd1 == 0xCAFEBABE);

    // Test writing to x0 doesn't do anything
    duv->we3 = 1;
    duv->wa3 = 0;
    duv->wd3 = 0x12345678;
    tick();
    duv->we3 = 0;
    duv->ra1 = 0;
    duv->eval();
    std::cout << "Read x0 after writing to x0: " << duv->rd1 << std::endl;
    assert(duv->rd1 == 0);

    delete duv;
    std::cout << "REGFILE TEST PASSED!" << std::endl;
    return 0;
}
