// tb_riscvmove.cpp
// Verilator testbench for the overall riscvmove CPU.
//
// riscvmove takes memclk (4x rate), clk (derived, memclk/4), and phase (the
// free-running 2-bit memclk-edge counter clk is generated from) -- see
// ice40hx8k.sv and riscvmove.sv's header comment for the full timing
// picture. This testbench reproduces that exact relationship in software:
// one clk period is 4 memclk periods (4 posedges + 4 negedges), with
// phase/clk updated at each memclk posedge using the same pre-edge-value
// semantics real hardware has (ordinary Verilog NBA), so riscvmove's
// `phase == 2'd2` gating sees exactly the same pre-edge values it would on
// the board.

#include "Vriscvmove.h"
#include "verilated.h"
#include "verilated_fst_c.h"
#include <cstdint>
#include <iostream>
#include <fstream>
#include <cassert>

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);
    Vriscvmove* duv = new Vriscvmove;

    VerilatedFstC* tfp = new VerilatedFstC;
    duv->trace(tfp, 99);
    tfp->open("waves/cputest.fst");
    uint64_t sim_time = 0;
    auto step = [&]() {
        duv->eval();
        tfp->dump(sim_time);
        sim_time++;
    };

    // Start just before phase would wrap to 0, so the first memclk_posedge()
    // call below correctly represents phase 0 (clk's own rising edge).
    int phase_state = 3;
    duv->memclk = 0;
    duv->phase = phase_state;
    duv->clk = 0;

    // One memclk rising edge. Mirrors real hardware's two-step settling:
    // memclk transitions while phase/clk still hold their pre-edge values
    // (what any memclk-triggered logic reading them at this instant would
    // see), then phase/clk update, same as the real registers would.
    auto memclk_posedge = [&]() {
        duv->memclk = 1;
        step();
        phase_state = (phase_state + 1) % 4;
        duv->phase = phase_state;
        duv->clk = (phase_state == 0 || phase_state == 1) ? 1 : 0;
        step();
    };

    auto memclk_negedge = [&]() {
        duv->memclk = 0;
        step();
    };

    // One full clk period: 4 memclk periods. phase 0's posedge commits
    // pc/regfile/halt_reg; the following negedge is imem's fetch capture;
    // phase 2's posedge is dmem's capture and the mmio_we/mmio_re commit
    // point; the rest is margin.
    auto tick = [&]() {
        for (int i = 0; i < 4; i++) {
            memclk_posedge();
            memclk_negedge();
        }
    };

    // Reset sequence.
    duv->reset = 1;
    tick();
    duv->reset = 0;
    step();

    std::cout << "Starting CPU Simulation...\n";

    // Run simulation for up to 100 cycles or until halt
    int cycles = 0;
    bool x4_correct = false;
    while (cycles < 100) {
        tick();
        cycles++;

        // Print state for debugging
        if (duv->monitor_wb_we && duv->monitor_wb_reg != 0) {
            std::cout << "Cycle " << cycles << ": Reg x" << (int)duv->monitor_wb_reg
                      << " <= 0x" << std::hex << duv->monitor_wb_data << std::dec << std::endl;
            if (duv->monitor_wb_reg == 4 && duv->monitor_wb_data == 30) {
                x4_correct = true;
            }
        }

        if (duv->monitor_halt) {
            std::cout << "Halt signal detected at cycle " << cycles << "\n";
            break;
        }
    }

    std::cout << "Simulation completed in " << cycles << " cycles.\n";
    assert(x4_correct);
    std::cout << "Verified: Register x4 was successfully loaded with 30!\n";

    tfp->close();
    delete tfp;
    delete duv;
    std::cout << "RISCVMOVE CPU TEST PASSED!" << std::endl;
    return 0;
}
