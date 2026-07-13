// alu.sv

`ifndef ALU_SV
`define ALU_SV

`include "riscvmove_defs.sv"

module alu (
  input  logic [3:0]   op,
  input  logic [31:0]  a,
  input  logic [31:0]  b,
  output logic [31:0]  result,
  output logic         zero,
  output logic         less,   // Signed less-than
  output logic         lessu   // Unsigned less-than
);



endmodule

`endif
