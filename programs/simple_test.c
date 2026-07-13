#include <stdint.h>

#define UART_TX     ((volatile char *)0x80000000)
#define UART_RX     ((volatile char *)0x80000004)
#define UART_STATUS ((volatile int *) 0x80000008)

#define UART_STATUS_TX_READY 0x1
#define UART_STATUS_RX_READY 0x2

void uart_putchar(char c) {
    *UART_TX = c;
}

int main() {
    for (volatile int i = 0; i < 10; i++) {
        uart_putchar('H');
        uart_putchar('i');
        uart_putchar('!');
        uart_putchar('\n');
    }
    asm volatile("ecall");
    return 0;
}
