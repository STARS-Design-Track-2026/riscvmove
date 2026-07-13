`include "riscvmove_defs.sv"
`include "riscvmove.sv"

`default_nettype none

module top (
  // I/O ports
  // memclk is the board's raw 12MHz oscillator, used directly to drive
  // imem/dmem's real block RAM (see riscvmove.sv's header comment). clk is
  // memclk divided by 4 (3MHz) -- the CPU's own single-cycle clock: pc,
  // regfile, and halt_reg all commit once per clk edge. phase is the
  // free-running 2-bit memclk-edge counter clk is derived from -- riscvmove
  // needs it directly to know exactly which memclk edge within one clk
  // period is safe to commit dmem/MMIO writes on.
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

  // UART MMIO interface
  // 0x80000000: UART Data (R: rxdata, W: txdata)
  // 0x80000004: UART Status (bit 0: txready, bit 1: rxready)
  
  logic uart_tx_en;
  assign txclk = uart_tx_en;

  // Clocked by memclk (the fast clock), not clk -- mmio_we/mmio_re are
  // already gated to be high for exactly one memclk edge per instruction
  // (see riscvmove.sv), so sampling them here at memclk's rate picks that
  // edge up immediately rather than waiting an extra clk period.
  always_ff @(posedge memclk) begin
    if (reset) begin
      left_reg  <= 8'b0;
      right_reg <= 8'b0;
      for (int i=0; i<8; i++) ss_reg[i] <= 8'b0;
      rgb_reg   <= 3'b0;
      uart_tx_en <= 1'b0;
      txdata <= 8'b0;
    end else begin
      uart_tx_en <= 1'b0;
      if (mmio_we) begin
        case (mmio_addr)
          32'h80000000: begin // UART TX
            // txready gate is a safety net against software that forgets to
            // poll UART_STATUS -- it drops an unready write instead of
            // corrupting an in-flight transmission. Software must still
            // poll, since the CPU does not stall on this write.
            if (mmio_be[0] && txready) begin
              txdata <= mmio_dout[7:0];
              uart_tx_en <= 1'b1;
            end
          end
          32'h80000010: begin // LEDs Left
            if (mmio_be[0]) left_reg <= mmio_dout[7:0];
          end
          32'h80000014: begin // LEDs Right
            if (mmio_be[0]) right_reg <= mmio_dout[7:0];
          end
          32'h80000020: if (mmio_be[0]) ss_reg[0] <= mmio_dout[7:0];
          32'h80000024: if (mmio_be[0]) ss_reg[1] <= mmio_dout[7:0];
          32'h80000028: if (mmio_be[0]) ss_reg[2] <= mmio_dout[7:0];
          32'h8000002c: if (mmio_be[0]) ss_reg[3] <= mmio_dout[7:0];
          32'h80000030: if (mmio_be[0]) ss_reg[4] <= mmio_dout[7:0];
          32'h80000034: if (mmio_be[0]) ss_reg[5] <= mmio_dout[7:0];
          32'h80000038: if (mmio_be[0]) ss_reg[6] <= mmio_dout[7:0];
          32'h8000003c: if (mmio_be[0]) ss_reg[7] <= mmio_dout[7:0];
          32'h80000040: if (mmio_be[0]) rgb_reg <= mmio_dout[2:0];
          default: ;
        endcase
      end
    end
  end

  // UART RX Ack
  // Gated on mmio_re (a genuine load instruction is reading this address),
  // not on "!mmio_we". mmio_addr reflects the MEM-stage ALU result for
  // *every* instruction, not just loads/stores -- e.g. every "lui
  // a5,0x80000" used to build a peripheral address transiently puts
  // 0x80000000 on mmio_addr with mmio_we=0, which looked identical to a real
  // read here. Since that boilerplate precedes almost every MMIO access in
  // practice, this falsely acked (and silently dropped) received bytes
  // before the program's actual read ever executed. Also still gated on
  // rxready, so a 0x00 byte (mmio_din==0) is acked correctly too.
  assign rxclk = (mmio_addr == 32'h80000000 && mmio_re && rxready);

  always_comb begin
    mmio_din = 32'b0;
    case (mmio_addr)
      32'h80000000: mmio_din = {24'b0, rxdata};
      32'h80000004: mmio_din = {30'b0, rxready, txready};
      32'h80000050: mmio_din = {11'b0, pb}; // Pushbuttons
      default: mmio_din = 32'b0;
    endcase
  end

endmodule
