// regfile.sv

`ifndef REGFILE_SV
`define REGFILE_SV

module regfile (
  input  logic        clk,
  input  logic        we3,
  input  logic [4:0]  ra1,
  input  logic [4:0]  ra2,
  input  logic [4:0]  wa3,
  input  logic [31:0] wd3,
  output logic [31:0] rd1,
  output logic [31:0] rd2
);



endmodule

`endif
