// calculator.c
// Pushbutton four-function calculator.
//   pb[0..9]  digits 0-9. The first digit after an operator/equals (or at
//             startup) starts a fresh number, overwriting the display;
//             further digits before the next operator append to build a
//             multi-digit operand (1, 2 -> "12").
//   pb[19]    + (add)
//   pb[18]    - (subtract)
//   pb[17]    * (multiply)
//   pb[16]    = (evaluate)
// Pressing an operator applies whatever operation is still pending against
// the digit just entered (seeding the accumulator on the very first one),
// then leaves that operator pending for next time -- so "5 + 9 * 3 ="
// evaluates left-to-right as (5+9)*3, like a simple four-function
// calculator, not a scientific one with operator precedence. After "=" the
// result stays in the accumulator, so pressing another operator and digit
// continues from it. Pressing "=" again with no new digit/operator in
// between repeats the last operation against the current result (5+9=14,
// then "=" again gives 14+9=23, then 32, ...).
//
// This CPU is RV32I only (no hardware multiply/divide), so multiplication
// and the decimal conversion needed to display results use hand-rolled
// shift-add/shift-subtract routines instead of relying on the toolchain.

#define PB ((volatile unsigned int *)0x80000050)
#define SS(n) (*(volatile unsigned char *)(0x80000020 + 4 * (n)))

// Active-high seven-segment font, bit0=A,bit1=B,bit2=C,bit3=D,bit4=E,bit5=F,bit6=G.
static const unsigned char seg7[16] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
    0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71
};
#define SEG_MINUS 0x40 // just segment G lit

// Shift-add long multiplication: RV32I has no MUL instruction.
static int int_mul(int a, int b) {
    int result = 0;
    unsigned int ua = (unsigned int)a;
    while (ua) {
        if (ua & 1u) result += b;
        b <<= 1;
        ua >>= 1;
    }
    return result;
}

// Restoring binary long division: needed to peel off decimal digits for
// display, since RV32I has no DIV instruction either.
static unsigned int uint_divmod(unsigned int dividend, unsigned int divisor, unsigned int *rem_out) {
    unsigned int quotient = 0;
    unsigned int remainder = 0;
    for (int i = 31; i >= 0; i--) {
        remainder = (remainder << 1) | ((dividend >> i) & 1u);
        if (remainder >= divisor) {
            remainder -= divisor;
            quotient |= (1u << i);
        }
    }
    if (rem_out) *rem_out = remainder;
    return quotient;
}

static void display_number(int value) {
    int negative = (value < 0);
    unsigned int u = negative ? (unsigned int)(-value) : (unsigned int)value;
    unsigned char digits[8];
    int count = 0;

    if (u == 0) {
        digits[count++] = 0;
    } else {
        while (u > 0 && count < 8) {
            unsigned int rem;
            u = uint_divmod(u, 10, &rem);
            digits[count++] = (unsigned char)rem;
        }
    }

    for (int i = 0; i < 8; i++) SS(i) = 0x00;

    int pos = 0;
    for (; pos < count; pos++) SS(pos) = seg7[digits[pos]];
    if (negative && pos < 8) SS(pos) = SEG_MINUS;
}

enum { OP_NONE, OP_ADD, OP_SUB, OP_MUL };

static int apply_op(int op, int a, int b) {
    switch (op) {
        case OP_ADD: return a + b;
        case OP_SUB: return a - b;
        case OP_MUL: return int_mul(a, b);
        default:     return a;
    }
}

int main() {
    int accumulator = 0;
    int current_operand = 0;
    int have_operand = 0; // an operand has been entered since the last op/equals
    int op_pending = OP_NONE;
    int last_op = OP_NONE;     // the operator actually applied most recently
    int last_operand = 0;      // its operand, used to repeat on a bare "="
    unsigned int prev_pb = 0;

    display_number(0);

    while (1) {
        unsigned int pb = *PB;
        unsigned int pressed = pb & ~prev_pb; // rising-edge detect
        prev_pb = pb;

        for (int d = 0; d <= 9; d++) {
            if (pressed & (1u << d)) {
                // have_operand is false right after an operator/equals (or at
                // startup) -- the first digit of a new number starts fresh,
                // overwriting whatever was displayed; subsequent digits
                // append onto it to build a multi-digit number.
                current_operand = have_operand ? current_operand * 10 + d : d;
                have_operand = 1;
                display_number(current_operand);
            }
        }

        int new_op = OP_NONE;
        int op_pressed = 0;
        if (pressed & (1u << 19)) { new_op = OP_ADD; op_pressed = 1; }
        if (pressed & (1u << 18)) { new_op = OP_SUB; op_pressed = 1; }
        if (pressed & (1u << 17)) { new_op = OP_MUL; op_pressed = 1; }
        int equals_pressed = (pressed & (1u << 16)) != 0;

        if (op_pressed || equals_pressed) {
            if (op_pending != OP_NONE) {
                if (have_operand) {
                    accumulator = apply_op(op_pending, accumulator, current_operand);
                    last_op = op_pending;
                    last_operand = current_operand;
                }
            } else if (have_operand) {
                // first operand ever entered -- seed the accumulator, there's
                // no operation yet for a later bare "=" to repeat
                accumulator = current_operand;
            } else if (equals_pressed && last_op != OP_NONE) {
                // bare "=" with nothing new entered: repeat the last operation
                accumulator = apply_op(last_op, accumulator, last_operand);
            }
            have_operand = 0;
            op_pending = equals_pressed ? OP_NONE : new_op;
            display_number(accumulator);
        }

        // Crude debounce: a few thousand cycles between polls so a single
        // physical press (which lasts far longer than this) doesn't get
        // sampled as a burst of edges from contact bounce.
        for (volatile unsigned int i = 0; i < 20000u; i++);
    }

    return 0;
}
