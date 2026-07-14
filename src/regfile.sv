// regfile.sv

`ifndef REGFILE_SV
`define REGFILE_SV

module regfile (
  input  logic        clk,
  input  logic        reset,
  input  logic        we,
  input  logic [4:0]  rs1_idx,
  input  logic [4:0]  rs2_idx,
  input  logic [4:0]  rd_idx,
  input  logic [31:0] rd,
  output logic [31:0] rs1,
  output logic [31:0] rs2
);



endmodule

`endif
