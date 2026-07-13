// imm_gen.sv
// Immediate builder for the riscvmove CPU.
//
// Purely combinational: reconstructs the sign-extended 32-bit immediate
// from whichever bits of the current instruction hold it, picking the
// layout by opcode (RV32I scatters the immediate's bits across different
// instruction fields depending on format -- I/S/B/U/J).

`ifndef IMM_GEN_SV
`define IMM_GEN_SV

module imm_gen (
  input  logic [31:0] instr,
  output logic [31:0] imm
);



endmodule

`endif
