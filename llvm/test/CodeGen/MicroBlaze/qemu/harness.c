// Bare-metal integration test for petalogix-s3adsp1800 (MicroBlaze LE).
// Tests compiled C functions and writes 'P' (pass) or 'F' (fail) to the
// uartlite TX FIFO at 0x84000004.

#include "add.c"

static void uart_putc(int c) {
    volatile int *tx = (volatile int *)0x84000004;
    *tx = c;
}

int main(void) {
    // Test 1: add(3, 4) == 7
    if (add(3, 4) != 7) { uart_putc('F'); uart_putc('\n'); return 1; }

    // Test 2: add(-5, 5) == 0
    if (add(-5, 5) != 0) { uart_putc('F'); uart_putc('\n'); return 1; }

    // Test 3: sub(10, 3) == 7
    if (sub(10, 3) != 7) { uart_putc('F'); uart_putc('\n'); return 1; }

    // Test 4: sub(0, -1) == 1
    if (sub(0, -1) != 1) { uart_putc('F'); uart_putc('\n'); return 1; }

    uart_putc('P');
    uart_putc('\n');
    return 0;
}
