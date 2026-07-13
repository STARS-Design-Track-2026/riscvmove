// Verilator testbench for led_test.c running on riscvmove.
//
// This exercises the MMIO store path that the board-level LED demo depends
// on. It mirrors the memclk/clk/phase relationship from tb_riscvmove.cpp and
// samples MMIO writes at the same pre-edge instant a real synchronous
// peripheral would.

#include "Vriscvmove.h"
#include "verilated.h"
#include "verilated_fst_c.h"
#include <cassert>
#include <cstdint>
#include <iostream>

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);
    Vriscvmove* duv = new Vriscvmove;

    VerilatedFstC* tfp = new VerilatedFstC;
    duv->trace(tfp, 99);
    tfp->open("waves/ledtest.fst");
    uint64_t sim_time = 0;
    auto step = [&]() {
        duv->eval();
        tfp->dump(sim_time);
        sim_time++;
    };

    int phase_state = 3;
    duv->memclk = 0;
    duv->phase = phase_state;
    duv->clk = 0;
    duv->mmio_din = 0;

    bool saw_left = false;
    bool saw_right = false;
    bool saw_ss0 = false;
    bool saw_rgb = false;

    auto memclk_posedge = [&]() {
        duv->memclk = 1;
        step();

        if (phase_state == 2 && duv->mmio_we) {
            const uint32_t addr = duv->mmio_addr;
            const uint8_t data = static_cast<uint8_t>(duv->mmio_dout & 0xff);

            if (addr == 0x80000010 && data == 0x01) saw_left = true;
            if (addr == 0x80000014 && data == 0x80) saw_right = true;
            if (addr == 0x80000020 && data == 0x3f) saw_ss0 = true;
            if (addr == 0x80000040) saw_rgb = true;
        }

        phase_state = (phase_state + 1) % 4;
        duv->phase = phase_state;
        duv->clk = (phase_state == 0 || phase_state == 1) ? 1 : 0;
        step();
    };

    auto memclk_negedge = [&]() {
        duv->memclk = 0;
        duv->mmio_din = (duv->mmio_addr == 0x80000050) ? 0 : 0;
        step();
    };

    auto tick = [&]() {
        for (int i = 0; i < 4; i++) {
            memclk_posedge();
            memclk_negedge();
        }
    };

    duv->reset = 1;
    tick();
    duv->reset = 0;
    step();

    for (int cycles = 0; cycles < 200; cycles++) {
        tick();
        if (saw_left && saw_right && saw_ss0 && saw_rgb) break;
    }

    assert(saw_left);
    assert(saw_right);
    assert(saw_ss0);
    assert(saw_rgb);

    tfp->close();
    delete tfp;
    delete duv;
    std::cout << "LED TEST PROGRAM PASSED!" << std::endl;
    return 0;
}