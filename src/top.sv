`include "riscvmove_defs.sv"
`include "riscvmove.sv"

`default_nettype none

module top (
  // I/O ports
  input  logic memclk, clk, reset,
  input  logic [1:0] phase,
  input  logic [20:0] pb,
  output logic [7:0] left, right,
         ss7, ss6, ss5, ss4, ss3, ss2, ss1, ss0,
  output logic red, green, blue,

  // UART ports
  output logic [7:0] txdata,
  input  logic [7:0] rxdata,
  output logic txclk, rxclk,
  input  logic txready, rxready
);

  // CPU signals
  logic [31:0] monitor_pc;
  logic [31:0] monitor_wb_data;
  logic [4:0]  monitor_wb_reg;
  logic        monitor_wb_we;
  logic        monitor_halt;

  logic [31:0] mmio_addr;
  logic [31:0] mmio_dout;
  logic [3:0]  mmio_be;
  logic        mmio_we;
  logic        mmio_re;
  logic [31:0] mmio_din;

  // Peripheral registers for MMIO
  logic [7:0] left_reg, right_reg;
  logic [7:0] ss_reg [7:0];
  logic [2:0] rgb_reg;

  assign left  = left_reg;
  assign right = right_reg;
  assign ss0   = ss_reg[0];
  assign ss1   = ss_reg[1];
  assign ss2   = ss_reg[2];
  assign ss3   = ss_reg[3];
  assign ss4   = ss_reg[4];
  assign ss5   = ss_reg[5];
  assign ss6   = ss_reg[6];
  assign ss7   = ss_reg[7];
  assign {red, green, blue} = rgb_reg;

  // CPU Instance
  riscvmove #(
    .INIT_FILE("build/test_program.hex"),
    .DATA_INIT_FILE("build/test_program_data.hex")
  ) cpu_inst (
    .clk(clk),
    .memclk(memclk),
    .phase(phase),
    .reset(reset),
    .monitor_pc(monitor_pc),
    .monitor_wb_data(monitor_wb_data),
    .monitor_wb_reg(monitor_wb_reg),
    .monitor_wb_we(monitor_wb_we),
    .monitor_halt(monitor_halt),
    .mmio_addr(mmio_addr),
    .mmio_dout(mmio_dout),
    .mmio_be(mmio_be),
    .mmio_we(mmio_we),
    .mmio_re(mmio_re),
    .mmio_din(mmio_din)
  );

  //
  // Implement your MMIO interfaces in this file.
  // 

endmodule
