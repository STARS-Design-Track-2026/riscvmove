// tb_dmem.cpp
// Verilator testbench for the data RAM (registered read, synchronous
// byte-masked write -- real block RAM, clocked by memclk).

#include "Vdmem.h"
#include "verilated.h"
#include <iostream>
#include <cassert>

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vdmem* duv = new Vdmem;

    auto tick = [&]() {
        duv->memclk = 0;
        duv->eval();
        duv->memclk = 1;
        duv->eval();
    };

    duv->addr_b = 0;
    duv->din_b = 0;
    duv->be_b = 0;
    duv->we_b = 0;
    duv->eval();

    // Write commits on this edge, but the read is registered too -- dout_b
    // captures the *pre-write* value of mem[addr_b] on the same edge as the
    // write (both the write and the read happen in the same always_ff
    // block, using ordinary NBA semantics), not the value just written.
    duv->addr_b = 0x20;
    duv->din_b = 0x11223344;
    duv->be_b = 0xF;
    duv->we_b = 1;
    tick();

    // A second tick, address unchanged, we_b now low: dout_b now reflects
    // the write that just committed.
    duv->we_b = 0;
    tick();
    std::cout << "Read after write: 0x" << std::hex << duv->dout_b << std::dec << std::endl;
    assert(duv->dout_b == 0x11223344);

    // Partial byte write (byte-enable masking). 0x20 currently holds
    // 0x11223344; writing 0xAABBCCDD with be=0x2 (byte 1 only) should only
    // change bits [15:8]: expect 0x1122CC44 on the *following* tick.
    duv->din_b = 0xAABBCCDD;
    duv->be_b = 2;
    duv->we_b = 1;
    tick();

    duv->we_b = 0;
    tick();
    std::cout << "Read after byte-masked write: 0x" << std::hex << duv->dout_b << std::dec << std::endl;
    assert(duv->dout_b == 0x1122cc44);

    // A different, never-written address reads as 0 once its own registered
    // read has had a tick to capture it.
    duv->addr_b = 0x24;
    tick();
    assert(duv->dout_b == 0);

    delete duv;
    std::cout << "DMEM TEST PASSED!" << std::endl;
    return 0;
}
