// uart_sample.c
// A sample C program that interacts with MMIO UART

#define UART_TX     ((volatile char *)0x80000000)
#define UART_STATUS ((volatile int *) 0x80000004)
#define LED_LEFT    ((volatile char *)0x80000010)

void uart_putchar(char c) {
    while (!(*UART_STATUS & 0x1)); // wait for tx ready
    *UART_TX = c;
}

void uart_putstr(const char* s) {
    while (*s) {
        uart_putchar(*s++);
        *LED_LEFT = *LED_LEFT + 1; // bump LED count after each char actually sent
    }
}

void uart_getchar(char* c) {
    while (!(*UART_STATUS & 0x2)); // wait for rx ready
    *c = *UART_TX; // read character from UART
}

// This build is freestanding (-nostdlib, no libc) and this CPU is RV32I
// only (no M extension), so atoi/sprintf and any runtime '*'/'/' between two
// non-constant ints are all unavailable -- atoi/sprintf need libc (which
// itself needs syscall stubs we don't have), and a runtime multiply/divide
// would otherwise call into libgcc's __mulsi3/__udivsi3, which isn't linked
// in. Multiplying/dividing by a compile-time constant (e.g. n*10 below) is
// fine as-is -- the compiler strength-reduces that to shifts on its own.

// Replaces atoi(): buffer is already pre-validated as all-digit by the
// caller, so this just needs to accumulate, no sign/whitespace handling.
int str_to_int(const char *s) {
    int n = 0;
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0'); // '*10' is a compile-time constant, safe
        s++;
    }
    return n;
}

// Replaces the runtime 'factorial *= i' multiply: shift-add long
// multiplication, 32 steps worst case, using only shifts/add/compare.
int int_mul(int a, int b) {
    int result = 0;
    unsigned int ua = (unsigned int)a;
    while (ua) {
        if (ua & 1) result += b;
        b <<= 1;
        ua >>= 1;
    }
    return result;
}

// Restoring binary long division, 32 steps, using only shifts/compare/sub --
// needed because printing decimal digits requires dividing by 10 at runtime.
unsigned int uint_divmod(unsigned int dividend, unsigned int divisor, unsigned int *rem_out) {
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

// Replaces sprintf's "%d": prints n in decimal directly over UART.
void uart_putint(int n) {
    char digits[12];
    int count = 0;
    unsigned int u;

    if (n < 0) {
        uart_putchar('-');
        u = (unsigned int)(-n);
    } else {
        u = (unsigned int)n;
    }

    if (u == 0) {
        uart_putchar('0');
        return;
    }

    while (u > 0) {
        unsigned int rem;
        u = uint_divmod(u, 10, &rem);
        digits[count++] = '0' + (char)rem;
    }
    while (count > 0) {
        uart_putchar(digits[--count]);
    }
}

int main() {
    // while(1) {
    //     *LED_LEFT = 0x01; // reached main, before any putchar
    //     uart_putstr("Hello Ben\r\n");
    //     for (volatile int i = 0; i < 500000; i++); // delay
    //     *LED_LEFT = 0xFF; // every putchar's wait-loop returned
    //     for (volatile int i = 0; i < 500000; i++); // delay
    // }

    // Write a program to continually print "Enter a number: " and wait for a 
    // number.  When a number is received, calculate its factorial, and 
    // print "You entered: " followed by "num! = factorial".  
    // Repeat this forever.
    for (volatile unsigned int i = 0; i < 100000u; i++);
    while (1) {
        uart_putstr("Enter a number: ");
        char buffer[10];
        int index = 0;
        char c;

        // Read characters until newline or buffer is full
        while (index < sizeof(buffer) - 1) {
            while (!(*UART_STATUS & 0x2)); // wait for rx ready
            c = *UART_TX; // read character from UART
            if (c == '\n' || c == '\r') {
                break; // end of input
            }
            buffer[index++] = c;
            uart_putchar(c); // echo the character back
        }
        buffer[index] = '\0'; // null-terminate the string

        // Convert string to integer
        int num = 0;
        for (int i = 0; i < index; i++) {
            if (buffer[i] < '0' || buffer[i] > '9') {
                uart_putstr("\r\nInvalid input. Please enter a number.\r\n");
                num = -1; // indicate invalid input
                break;
            }
        }
        num = (num == -1) ? -1 : str_to_int(buffer); // convert to integer if valid
        if (num != -1) {
            // Calculate factorial
            int factorial = 1;
            for (int i = 1; i <= num; i++) {
                factorial = int_mul(factorial, i);
            }

            // Print result
            uart_putstr("\r\nYou entered: ");
            uart_putint(num);
            uart_putstr("! = ");
            uart_putint(factorial);
            uart_putstr("\r\n");
        }
    }

    // Force halt via ecall
    asm volatile("ecall");
    return 0;
}
