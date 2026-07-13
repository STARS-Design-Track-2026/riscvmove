// tb_riscvmove_uart.cpp
// Verilator testbench for riscvmove CPU with UART MMIO simulation.
//
// See tb_riscvmove.cpp for the full explanation of the memclk/clk/phase
// relationship. This testbench additionally plays the role of top.sv's
// MMIO peripheral consumer: it samples mmio_we/mmio_addr/mmio_dout at the
// same pre-edge instant a real `always_ff @(posedge memclk)` block would
// (right when memclk rises into phase 2, before phase/clk themselves
// update), and drives mmio_din the way top.sv's combinational read mux
// would.

#include "Vriscvmove.h"
#include "verilated.h"
#include <iostream>
#include <string>
#include <queue>

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vriscvmove* duv = new Vriscvmove;

    int phase_state = 3;
    duv->memclk = 0;
    duv->phase = phase_state;
    duv->clk = 0;
    duv->mmio_din = 0;

    auto memclk_posedge = [&]() {
        duv->memclk = 1;
        duv->eval();

        // Sample mmio_we/mmio_addr/mmio_dout right here, at the instant
        // memclk rises but before phase/clk update -- this is exactly
        // when phase's pre-edge value determines whether this is the
        // phase-2 (commit) edge, mirroring a real synchronous peripheral.
        // mem_commit = (phase==2'd2) reads true using phase's pre-edge
        // value specifically at the edge that transitions phase 2->3 (not
        // 1->2) -- phase holds the value 2 for the whole interval *after*
        // becoming 2 and *until* this edge, so this is the one edge where
        // a real synchronous consumer's pre-edge sample sees phase==2.
        if (phase_state == 2 /* about to become 3 */) {
            if (duv->mmio_we && duv->mmio_addr == 0x80000000) {
                char c = (char)(duv->mmio_dout & 0xFF);
                std::cout << c << std::flush;
            }
        }

        phase_state = (phase_state + 1) % 4;
        duv->phase = phase_state;
        duv->clk = (phase_state == 0 || phase_state == 1) ? 1 : 0;
        duv->eval();
    };

    auto memclk_negedge = [&]() {
        duv->memclk = 0;
        duv->eval();

        // imem captures the instruction here (phase 0's negedge); once
        // instr is fresh, mmio_addr reflects the current instruction, in
        // time for the mux below to settle before phase 2 commits.
        duv->mmio_din = 0x0;
        if (duv->mmio_addr == 0x80000004) {
            duv->mmio_din = 0x1; // TX always ready
        }
        duv->eval();
    };

    auto tick = [&]() {
        for (int i = 0; i < 4; i++) {
            memclk_posedge();
            memclk_negedge();
        }
    };

    // Reset sequence
    duv->reset = 1;
    tick();
    duv->reset = 0;
    duv->eval();

    std::cout << "[SIM] Starting CPU Simulation...\n";
    std::cout << "--------------------------------------------------\n";

    // Run simulation for up to 100000 cycles or until halt
    int cycles = 0;

    while (cycles < 100000) {
        tick();
        cycles++;

        if (duv->monitor_halt) {
            std::cout << "\n--------------------------------------------------\n";
            std::cout << "[SIM] Halt signal detected at cycle " << cycles << "\n";
            break;
        }
    }

    if (cycles >= 100000) {
        std::cout << "\n[SIM] Timeout reached (100000 cycles).\n";
    }

    delete duv;
    std::cout << "[SIM] Simulation finished.\n";
    return 0;
}
