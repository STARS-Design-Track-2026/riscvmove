// riscvmove_defs.sv

// We provide you with the definitions and 
// constants for a standard RV32I pipelined CPU.

`ifndef RISCVMOVE_DEFS_SV
`define RISCVMOVE_DEFS_SV

package riscvmove_defs;

  // RV32I major opcodes
  typedef enum logic [6:0] {
    OP_LUI      = 7'b0110111,
    OP_AUIPC    = 7'b0010111,
    OP_JAL      = 7'b1101111,
    OP_JALR     = 7'b1100111,
    OP_BRANCH   = 7'b1100011,
    OP_LOAD     = 7'b0000011,
    OP_STORE    = 7'b0100011,
    OP_OP_IMM   = 7'b0010011,
    OP_OP       = 7'b0110011,
    OP_SYSTEM   = 7'b1110011
  } opcode_t;

  // ALU Operations
  typedef enum logic [3:0] {
    ALU_ADD    = 4'd0,
    ALU_SUB    = 4'd1,
    ALU_SLL    = 4'd2,
    ALU_SLT    = 4'd3,
    ALU_SLTU   = 4'd4,
    ALU_XOR    = 4'd5,
    ALU_SRL    = 4'd6,
    ALU_SRA    = 4'd7,
    ALU_OR     = 4'd8,
    ALU_AND    = 4'd9,
    ALU_COPY_B = 4'd10
  } alu_op_t;

  // Branch / Jump types
  typedef enum logic [2:0] {
    BR_BEQ  = 3'b000,
    BR_BNE  = 3'b001,
    BR_BLT  = 3'b100,
    BR_BGE  = 3'b101,
    BR_BLTU = 3'b110,
    BR_BGEU = 3'b111
  } branch_t;

endpackage

`endif
