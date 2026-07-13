// pc.sv
// Program counter for the riscvmove CPU

`ifndef PC_SV
`define PC_SV

module pc_reg (
  input  logic        clk,
  input  logic        reset,
  input  logic        halt,
  input  logic [31:0] next_pc,
  output logic [31:0] pc
);



endmodule

`endif
