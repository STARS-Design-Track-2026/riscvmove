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
    duv->reset = 0;
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

    // Test asynchronous reset: x1 and x2 both hold nonzero values from
    // earlier in this test (0xDEADBEEF, 0xCAFEBABE). Assert reset with NO
    // clock edge at all -- step() only calls eval(), it never toggles clk
    // -- to specifically prove this clears storage independent of clk, not
    // merely on the next rising edge the way a synchronous reset would.
    duv->we = 0;
    duv->reset = 1;
    step();
    duv->rs1_idx = 1;
    duv->rs2_idx = 2;
    step();
    std::cout << "Read x1, x2 immediately after reset (no clock edge): 0x"
              << std::hex << duv->rs1 << ", 0x" << duv->rs2 << std::dec << std::endl;
    assert(duv->rs1 == 0);
    assert(duv->rs2 == 0);

    // Reset should hold storage at 0 the whole time it's asserted, even
    // across clock edges with we high (reset must take priority over a
    // write, not race it).
    duv->we = 1;
    duv->rd_idx = 1;
    duv->rd = 0xFFFFFFFF;
    tick();
    duv->we = 0;
    step();
    std::cout << "Read x1 (write attempted while reset still held): 0x"
              << std::hex << duv->rs1 << std::dec << std::endl;
    assert(duv->rs1 == 0);

    // Deasserting reset should let normal writes resume immediately.
    duv->reset = 0;
    duv->we = 1;
    duv->rd_idx = 1;
    duv->rd = 0x11223344;
    tick();
    duv->we = 0;
    step();
    std::cout << "Read x1 (normal write after reset released): 0x"
              << std::hex << duv->rs1 << std::dec << std::endl;
    assert(duv->rs1 == 0x11223344);

    tfp->close();
    delete tfp;
    delete duv;
    std::cout << "REGFILE TEST PASSED!" << std::endl;
    return 0;
}
