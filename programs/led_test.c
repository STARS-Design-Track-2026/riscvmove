// led_test.c
// Johnson counters on left/right LEDs (opposite directions), a 0-F counter
// mirrored across all seven-segment displays with {red,green,blue} driven by
// the digit ANDed with 0b111, and a speed-up while any pushbutton is held.
//
// MMIO map used here (see PROGRAMS.md):
//   0x80000010  LEDs left  (8 bits)
//   0x80000014  LEDs right (8 bits)
//   0x80000020 + 4*n  seven-segment display n (n = 0..7), active-high
//   0x80000040  RGB (bit0=red, bit1=green, bit2=blue)
//   0x80000050  Pushbuttons, read-only, pb[20:0]

#define LED_LEFT  ((volatile unsigned char *)0x80000010)
#define LED_RIGHT ((volatile unsigned char *)0x80000014)
#define RGB       ((volatile unsigned char *)0x80000040)
#define PB        ((volatile unsigned int  *)0x80000050)
#define SS(n)     (*(volatile unsigned char *)(0x80000020 + 4 * (n)))

// Active-high seven-segment font, bit0=A,bit1=B,bit2=C,bit3=D,bit4=E,bit5=F,bit6=G.
static const unsigned char seg7[16] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
    0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71
};

// Tune these to taste -- they're just busy-wait iteration counts, calibrated
// for the CPU running at hwclk (12MHz). FAST is an 8x speedup.
#define NORMAL_DELAY 600000u
#define FAST_DELAY   75000u

int main() {
    unsigned char left_pat = 0x00;
    unsigned char right_pat = 0x00;
    unsigned char digit = 0;

    while (1) {
        // Johnson counter, growing/draining from bit0 toward bit7: the new
        // bit always enters at bit0 as the complement of the bit just
        // shifted out the top, so the moving edge travels left[0]->left[7].
        unsigned char left_msb = (left_pat >> 7) & 1u;
        left_pat = (unsigned char)((left_pat << 1) | (~left_msb & 1u));

        // Mirror image: new bit enters at bit7 as the complement of the bit
        // shifted out the bottom, so the moving edge travels right[7]->right[0].
        unsigned char right_lsb = right_pat & 1u;
        right_pat = (unsigned char)((right_pat >> 1) | ((~right_lsb & 1u) << 7));

        *LED_LEFT = left_pat;
        *LED_RIGHT = right_pat;

        unsigned char pattern = seg7[digit];
        for (int i = 0; i < 8; i++) SS(i) = pattern;
        *RGB = digit & 0x7u;
        digit = (digit + 1) & 0xFu;

        unsigned int delay = (*PB != 0) ? FAST_DELAY : NORMAL_DELAY;
        for (volatile unsigned int i = 0; i < delay; i++);
    }

    return 0;
}
