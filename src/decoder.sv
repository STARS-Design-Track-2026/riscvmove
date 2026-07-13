// decoder.sv
// Instruction decoder / control unit for the riscvmove CPU.
//
// Purely combinational: given the opcode/funct3/funct7 fields extracted
// from the current instruction, produces every downstream control signal
// (the main decoder) and the 4-bit ALU operation select (the ALU control
// decoder). No state, no clock -- one decode per instruction, same as
// every other block in this single-cycle datapath.

`ifndef DECODER_SV
`define DECODER_SV

module decoder (
  input  logic [6:0] opcode,
  input  logic [2:0] funct3,
  input  logic [6:0] funct7,

  output logic [3:0] alu_op,
  output logic       regwrite,
  output logic       alu_src_b,
  output logic       memtoreg,
  output logic       memwrite,
  output logic       branch,
  output logic       jal,
  output logic       jalr,
  output logic       lui,
  output logic       auipc
);

  

endmodule

`endif
