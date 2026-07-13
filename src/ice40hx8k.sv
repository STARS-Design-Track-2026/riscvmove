
module ice40hx8k (hwclk,pb,ss7,ss6,ss5,ss4,ss3,ss2,ss1,ss0,left,right,red,green,blue,
                  Rx, Tx, CTSn, DCDn);
    input hwclk;
    input [20:0] pb;
    output [7:0] ss7,ss6,ss5,ss4,ss3,ss2,ss1,ss0;
    output [7:0] left,right;
    output red,green,blue;
    input Rx;
    output Tx, CTSn, DCDn;

    // riscvmove needs two related clocks plus a phase count -- see
    // riscvmove.sv's header comment for the full reasoning. memclk is
    // hwclk directly (the raw 12MHz oscillator), used to drive imem/
    // dmem's real block RAM. clk is memclk divided by 4 (3MHz), the
    // CPU's own single-cycle clock: pc/regfile/halt_reg commit once per
    // clk edge. phase is a free-running 2-bit counter, incrementing every
    // memclk edge and wrapping 0->1->2->3->0 in lockstep with clk -- it's
    // how riscvmove.sv tells apart the four memclk edges within one clk
    // period (rather than just "is clk high"), so it can commit dmem/MMIO
    // writes at the *last* safe memclk edge instead of the first,
    // maximizing decode/ALU settling margin. All derived from memclk by
    // plain synchronous counting -- no second, independent oscillator
    // here, so there's no real clock-domain-crossing to worry about.
    wire memclk = hwclk;
    reg [1:0] phase = 2'd0;
    always_ff @(posedge memclk) phase <= phase + 2'd1;
    // Keep clk on a single registered bit, not a 2-bit decode. The old
    // `(phase == 0) || (phase == 1)` form has the same truth table, but it
    // turns clk into a multi-input combinational expression of two flops that
    // toggle together on 1->2 and 3->0. That is fine in zero-delay RTL
    // simulation, but it is a glitch-prone way to drive a real clock net on
    // FPGA fabric.
    wire clk = ~phase[1];

    assign CTSn = ~1; // clear to send
    assign DCDn = ~1; // carrier detect (makes Kermit happy)

// The UART IP's baud generator only needs a synchronous sample clock, not a
// separate PLL-derived domain. Using hwclk directly matches the known-good
// minicomp wrapper and removes an avoidable CDC between the CPU/MMIO pulses
// (generated from hwclk) and the UART itself.

    wire reset;
    wire xmit;
    wire [7:0] txdata;
    wire       txclk;
    wire       txready;
    wire recv;
    wire [7:0] rxdata;
    wire       rxclk;
    wire       rxready;

    uart uart_inst(
      .clk(hwclk),
      .rst(reset),
        .input_axis_tdata(txdata),
        .input_axis_tvalid(xmit),
        .input_axis_tready(txready),
        .output_axis_tdata(rxdata),
        .output_axis_tvalid(rxready),
        .output_axis_tready(recv),
        .rxd(Rx),
        .txd(Tx),
        .prescale(13)
    );

    // Direct handshake: top.sv already generates one-hwclk-wide pulses in the
    // same clock domain, so pass them straight into the UART instead of trying
    // to edge-detect/re-time them again.
    assign xmit = txclk;
    assign recv = rxclk;

    reset_on_start ros (reset, hwclk, pb[3] && pb[0] && pb[16]);
    // Named ports here deliberately, unlike this file's other instances --
    // top's port list mixes single-bit and multi-bit signals in a way that
    // silently bit-truncates/resizes on a positional mismatch (Yosys warns,
    // but doesn't error), instead of a clean width-mismatch error. That bit
    // Yosys once during this design's development; naming these avoids the
    // whole class of bug rather than relying on keeping two port lists in
    // sync by eye.
    top top_inst(
      .memclk(memclk),
      .clk(clk),
      .reset(reset),
      .phase(phase),
      .pb(pb),
      .left(left), .right(right),
      .ss7(ss7), .ss6(ss6), .ss5(ss5), .ss4(ss4),
      .ss3(ss3), .ss2(ss2), .ss1(ss1), .ss0(ss0),
      .red(red), .green(green), .blue(blue),
      .txdata(txdata),
      .rxdata(rxdata),
      .txclk(txclk), .rxclk(rxclk),
      .txready(txready), .rxready(rxready)
    );

endmodule

module reset_on_start(reset, clk, manual);
  output reset;
  input clk;
  input manual;

  // Keep reset high monotonically after power-up or a manual reset request,
  // then release it cleanly. The old sequence intentionally dropped reset low
  // for a moment to manufacture a later rising edge; that can let the CPU and
  // UART wrapper observe a partially-released state before the real reset
  // pulse, which is exactly the wrong behavior for startup I/O.
  reg [19:0] startup = 0;
  assign reset = ~startup[19] | manual;
  always @ (posedge clk, posedge manual)
    if (manual == 1)
      startup <= 0;
    else if (!startup[19])
      startup <= startup + 1'b1;

endmodule
