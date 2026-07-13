// tb_pc.cpp
// Verilator testbench for the program counter register (pc_reg)

#include "Vpc_reg.h"
#include "verilated.h"
#include "verilated_fst_c.h"
#include <cstdint>
#include <iostream>
#include <cassert>

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);
    Vpc_reg* duv = new Vpc_reg;

    VerilatedFstC* tfp = new VerilatedFstC;
    duv->trace(tfp, 99);
    tfp->open("waves/pc.fst");
    uint64_t sim_time = 0;
    auto step = [&]() {
        duv->eval();
        tfp->dump(sim_time);
        sim_time++;
    };

    auto tick = [&]() {
        duv->clk = 0;
        step();
        duv->clk = 1;
        step();
    };

    // Reset drives pc to 0 regardless of whatever next_pc is presented.
    duv->reset = 1;
    duv->halt = 0;
    duv->next_pc = 0xDEADBEEF;
    tick();
    std::cout << "After reset: pc = 0x" << std::hex << duv->pc << std::dec << std::endl;
    assert(duv->pc == 0);

    // Normal operation: once reset is released, pc follows next_pc each tick.
    duv->reset = 0;
    duv->next_pc = 4;
    tick();
    std::cout << "pc <= 4: pc = " << duv->pc << std::endl;
    assert(duv->pc == 4);

    duv->next_pc = 8;
    tick();
    std::cout << "pc <= 8: pc = " << duv->pc << std::endl;
    assert(duv->pc == 8);

    // Halt freezes pc in place even as next_pc keeps changing underneath it --
    // this is what lets the ecall-triggered halt latch in riscvmove.sv stop
    // the CPU without a dedicated "stop fetching" datapath.
    duv->halt = 1;
    duv->next_pc = 0x100;
    tick();
    std::cout << "Halted (next_pc=0x100): pc = 0x" << std::hex << duv->pc << std::dec << std::endl;
    assert(duv->pc == 8);

    duv->next_pc = 0x200;
    tick();
    std::cout << "Still halted (next_pc=0x200): pc = 0x" << std::hex << duv->pc << std::dec << std::endl;
    assert(duv->pc == 8);

    // Releasing halt resumes following next_pc from wherever it left off.
    duv->halt = 0;
    duv->next_pc = 0xC;
    tick();
    std::cout << "Resumed: pc = " << duv->pc << std::endl;
    assert(duv->pc == 0xC);

    // Reset takes priority over halt when both are asserted at once (the
    // RTL checks `if (reset) ... else if (!halt) ...`, so reset always wins).
    duv->halt = 1;
    duv->reset = 1;
    duv->next_pc = 0x1000;
    tick();
    std::cout << "Reset+halt together: pc = " << duv->pc << std::endl;
    assert(duv->pc == 0);

    tfp->close();
    delete tfp;
    delete duv;
    std::cout << "PC TEST PASSED!" << std::endl;
    return 0;
}
