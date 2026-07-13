// mem.sv

`ifndef MEM_SV
`define MEM_SV

module imem #(
  parameter INIT_FILE = "",
  parameter DEPTH     = 512
) (
  input  logic        memclk,
  input  logic [31:0] addr_a,
  output logic [31:0] dout_a
);



endmodule


module dmem #(
  parameter INIT_FILE = "",
  parameter DEPTH     = 256
) (
  input  logic        memclk,
  input  logic [31:0] addr_b,
  input  logic [31:0] din_b,
  input  logic [3:0]  be_b,
  input  logic        we_b, 
  output logic [31:0] dout_b
);



endmodule

`endif
