// riscvmove.sv

`ifndef RISCVMOVE_SV
`define RISCVMOVE_SV

`include "riscvmove_defs.sv"
`include "alu.sv"
`include "regfile.sv"
`include "mem.sv"
`include "pc.sv"
`include "decoder.sv"
`include "imm_gen.sv"

module riscvmove #(
  parameter INIT_FILE      = "", // .text/.rodata image for imem
  parameter DATA_INIT_FILE = ""  // .data image for dmem (no lw/sw path reaches imem, so dmem needs its own copy of .data's initial values)
) (
  input  logic        clk,
  input  logic        memclk,
  input  logic [1:0]  phase,
  input  logic        reset,

  // External monitoring ports for testbenches and wrapper
  output logic [31:0] monitor_pc,
  output logic [31:0] monitor_wb_data,
  output logic [4:0]  monitor_wb_reg,
  output logic        monitor_wb_we,
  output logic        monitor_halt,

  // Simple MMIO Interface
  output logic [31:0] mmio_addr,
  output logic [31:0] mmio_dout,
  output logic [3:0]  mmio_be,
  output logic        mmio_we,
  output logic        mmio_re,
  input  logic [31:0] mmio_din
);

  

endmodule

`endif
