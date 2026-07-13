// tb_regfile.cpp
// Verilator testbench for Register File

#include "Vregfile.h"
#include "verilated.h"
#include "verilated_fst_c.h"
#include <cstdint>
#include <iostream>
#include <cassert>

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);
    Vregfile* duv = new Vregfile;

    VerilatedFstC* tfp = new VerilatedFstC;
    duv->trace(tfp, 99);
    tfp->open("waves/regfile.fst");
    uint64_t sim_time = 0;
    auto step = [&]() {
        duv->eval();
        tfp->dump(sim_time);
        sim_time++;
    };

    // Helper lambda to toggle clock
    auto tick = [&]() {
        duv->clk = 0;
        step();
        duv->clk = 1;
        step();
    };

    // Initialize inputs
    duv->we = 0;
    duv->rs1_idx = 0;
    duv->rs2_idx = 0;
    duv->rd_idx = 0;
    duv->rd = 0;
    step();

    // Test x0 is always 0
    duv->rs1_idx = 0;
    step();
    std::cout << "Read x0: " << duv->rs1 << std::endl;
    assert(duv->rs1 == 0);

    // Write to x1
    duv->we = 1;
    duv->rd_idx = 1;
    duv->rd = 0xDEADBEEF;
    tick(); // Posited edge updates rf[1]

    // Read x1
    duv->we = 0;
    duv->rs1_idx = 1;
    step();
    std::cout << "Read x1 (normal): 0x" << std::hex << duv->rs1 << std::dec << std::endl;
    assert(duv->rs1 == 0xDEADBEEF);

    // Test no bypass: writing to x2 and reading from x2 in the same cycle
    // must see the OLD stored value (0, since x2 was never written before),
    // not the pending write data -- single-cycle riscvmove relies on this
    // for instructions like `addi x5,x5,1` that read and write the same
    // register in one cycle (see regfile.sv for why a bypass would instead
    // create a combinational loop there).
    duv->we = 1;
    duv->rd_idx = 2;
    duv->rd = 0xCAFEBABE;
    duv->rs1_idx = 2; // read same cycle
    step();
    std::cout << "Read x2 (same cycle as write, no bypass): 0x" << std::hex << duv->rs1 << std::dec << std::endl;
    assert(duv->rs1 == 0);

    tick(); // actually write it
    duv->we = 0;
    step();
    std::cout << "Read x2 (after clock): 0x" << std::hex << duv->rs1 << std::dec << std::endl;
    assert(duv->rs1 == 0xCAFEBABE);

    // Test writing to x0 doesn't do anything
    duv->we = 1;
    duv->rd_idx = 0;
    duv->rd = 0x12345678;
    tick();
    duv->we = 0;
    duv->rs1_idx = 0;
    step();
    std::cout << "Read x0 after writing to x0: " << duv->rs1 << std::endl;
    assert(duv->rs1 == 0);

    tfp->close();
    delete tfp;
    delete duv;
    std::cout << "REGFILE TEST PASSED!" << std::endl;
    return 0;
}
