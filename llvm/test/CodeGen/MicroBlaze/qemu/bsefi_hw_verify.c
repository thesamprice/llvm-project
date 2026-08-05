/*
 * bsefi_hw_verify.c — hardware verification for BSEFI/BSIFI instructions.
 *
 * Motivation: our LLVM backend was fixed in QEMU alongside this test after we
 * found that QEMU's gen_bsefi treated instruction bits[10:6] as the field WIDTH
 * instead of the END BIT (inclusive), which is what UG984 §5 specifies.  This
 * test verifies that real MicroBlaze silicon agrees with UG984 (end-bit encoding).
 *
 * BSEFI semantics per UG984 §5 Fig 82:
 *   bsefi rD, rA, END, START
 *   rD = rA[END:START]             (right-justified, zero-extended)
 *   Encoding: instruction bits[10:6] = END, bits[4:0] = START
 *
 * BSIFI semantics per UG984 §5 Fig 82:
 *   bsifi rD, rA, END, START       (assembler takes END = START + WIDTH - 1)
 *   rD[END:START] = rA[WIDTH-1:0]  (all other bits of rD unchanged)
 *   Note: our Clang assembler takes WIDTH as the third operand and encodes END;
 *   real hardware and GAS use the same convention.
 *
 * === Build instructions ===
 *
 * Compile with your LLVM toolchain (barrel-shift must be enabled):
 *
 *   MB_ROOT=~/src/claude-mb
 *   CLANG=$MB_ROOT/llvm-build/bin/clang
 *   LLVMMC=$MB_ROOT/llvm-build/bin/llvm-mc
 *   LLD=$MB_ROOT/llvm-build/bin/ld.lld
 *   QEMU_DIR=$MB_ROOT/llvm-project/llvm/test/CodeGen/MicroBlaze/qemu
 *
 *   $LLVMMC -filetype=obj -triple=microblazeel-unknown-elf \
 *       $QEMU_DIR/crt0.s -o /tmp/crt0.o
 *
 *   $CLANG --target=microblazeel-unknown-elf -O1 -ffreestanding -nostdlib \
 *       -Xclang -target-feature -Xclang +barrel-shift \
 *       -c $QEMU_DIR/bsefi_hw_verify.c -o /tmp/bsefi_hw_verify.o
 *
 *   $LLD -T $QEMU_DIR/linker.ld --entry=_start \
 *       /tmp/crt0.o /tmp/bsefi_hw_verify.o -o /tmp/bsefi_hw_verify.elf
 *
 * === Run on QEMU (quick smoke check) ===
 *
 *   qemu-system-microblazeel -M petalogix-s3adsp1800 \
 *       -kernel /tmp/bsefi_hw_verify.elf -nographic -serial mon:stdio
 *   Expected output: "PASS 11/11 bsefi/bsifi ok\n"
 *
 * === Run on hardware ===
 *
 * The UART TX FIFO address defaults to 0x84000004 (petalogix uartlite).
 * If your board maps the uartlite differently, recompile with:
 *   -DUART_TX_ADDR=0x<your_uart_tx_fifo_addr>
 *
 * Load the ELF via XMD / JTAG / TFTP and read the UART.
 * "PASS 11/11 bsefi/bsifi ok" = hardware agrees with UG984.
 * Any "FAIL" line identifies which specific extract/insert broke.
 *
 * === What the test proves ===
 *
 * If the hardware prints PASS with our encoding (END BIT in bits[10:6]),
 * then real MicroBlaze uses end-bit semantics and our LLVM backend + the
 * QEMU fix are both correct.
 */

/* Configurable UART TX FIFO address (uartlite at base + 4). */
#ifndef UART_TX_ADDR
#define UART_TX_ADDR 0x84000004u
#endif
#define UART_TX ((volatile unsigned int *)UART_TX_ADDR)

/* ── inline UART helpers ─────────────────────────────────────────────────── */

static void uart_putc(int c) { *UART_TX = (unsigned int)(unsigned char)c; }

static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }


static void uart_puthex(unsigned int v) {
    static const char h[] = "0123456789abcdef";
    uart_puts("0x");
    for (int i = 28; i >= 0; i -= 4)
        uart_putc(h[(v >> i) & 0xFu]);
}

/* ── inline-asm BSEFI / BSIFI ────────────────────────────────────────────── */

/*
 * BSEFI_IMMS(src, WIDTH, START) — force a specific BSEFI instruction at runtime.
 *
 * WIDTH and START must be integer literals (they are stringified into the asm).
 * GAS / hardware encoding: bits[10:6] = WIDTH, bits[4:0] = START.
 * Result = (src >> START) & ((1 << WIDTH) - 1).
 */
#define BSEFI_IMMS(src, WIDTH, START) \
    ({ unsigned int _r; \
       __asm__ volatile("bsefi %0, %1, " #WIDTH ", " #START \
                        : "=r"(_r) : "r"((unsigned int)(src))); \
       _r; })

/*
 * BSIFI_IMMS(base, src, WIDTH, START) — force a BSIFI instruction.
 *
 * Our Clang assembler (matching GAS) takes WIDTH as the third operand and
 * encodes END_BIT = START + WIDTH - 1 in bits[10:6].  QEMU and hardware
 * then compute width = bits[10:6] - bits[4:0] + 1 to recover WIDTH.
 *
 * Per UG984: result = base with result[START+WIDTH-1:START] = src[WIDTH-1:0].
 */
#define BSIFI_IMMS(base, src, WIDTH, START) \
    ({ unsigned int _r = (unsigned int)(base); \
       __asm__ volatile("bsifi %0, %1, " #WIDTH ", " #START \
                        : "+r"(_r) : "r"((unsigned int)(src))); \
       _r; })

/* ── test framework ──────────────────────────────────────────────────────── */

static int g_fail;

static void check(const char *name, unsigned int got, unsigned int want) {
    if (got != want) {
        g_fail++;
        uart_puts("FAIL ");
        uart_puts(name);
        uart_puts(": got=");
        uart_puthex(got);
        uart_puts(" want=");
        uart_puthex(want);
        uart_puts("\n");
    }
}

/* ── main ────────────────────────────────────────────────────────────────── */

int main(void) {
    g_fail = 0;

    /*
     * Reference value: 0x5466df2a
     *  nibble 7 (bits[31:28]) = 5
     *  nibble 6 (bits[27:24]) = 4  ← this was the failing case in uart_print_hex32
     *  nibble 5 (bits[23:20]) = 6
     *  nibble 4 (bits[19:16]) = 6
     *  nibble 3 (bits[15:12]) = d
     *  nibble 2 (bits[11: 8]) = f
     *  nibble 1 (bits[ 7: 4]) = 2
     *  nibble 0 (bits[ 3: 0]) = a
     */
    volatile unsigned int v1 = 0x5466df2au;

    /* Nibble extractions — 4 bits at each nibble position.
     * Syntax: bsefi rD, rA, WIDTH, START  (WIDTH=4 for nibble, START=nibble*4) */
    check("bsefi_nibble0_w4_s0",  BSEFI_IMMS(v1, 4,  0), 0xau);
    check("bsefi_nibble1_w4_s4",  BSEFI_IMMS(v1, 4,  4), 0x2u);
    check("bsefi_nibble2_w4_s8",  BSEFI_IMMS(v1, 4,  8), 0xfu);
    check("bsefi_nibble3_w4_s12", BSEFI_IMMS(v1, 4, 12), 0xdu);
    check("bsefi_nibble4_w4_s16", BSEFI_IMMS(v1, 4, 16), 0x6u);
    check("bsefi_nibble5_w4_s20", BSEFI_IMMS(v1, 4, 20), 0x6u);
    check("bsefi_nibble6_w4_s24", BSEFI_IMMS(v1, 4, 24), 0x4u); /* the bug case */
    check("bsefi_nibble7_w4_s28", BSEFI_IMMS(v1, 4, 28), 0x5u);

    /* Byte-wide extract: 8 bits starting at bit 8 of 0xDEADBEEF = 0xBE */
    volatile unsigned int v2 = 0xDEADBEEFu;
    check("bsefi_byte_w8_s8", BSEFI_IMMS(v2, 8, 8), 0xBEu);

    /* Cross-nibble field: 8 bits starting at bit 4 of 0xABCD1234 = 0x23 */
    volatile unsigned int v3 = 0xABCD1234u;
    check("bsefi_cross_w8_s4", BSEFI_IMMS(v3, 8, 4), 0x23u);

    /* Wide extract: 31 bits starting at bit 0 of 0xDEADBEEF = 0x5EADBEEFu */
    check("bsefi_wide_w31_s0", BSEFI_IMMS(v2, 31, 0), 0x5EADBEEFu);

    /*
     * BSIFI — insert low WIDTH bits of src into base at position START.
     *
     * Insert 0xA (4 bits) at position 4 of 0x00000000:
     *   result[7:4] = 0xA  →  0x000000A0
     */
    volatile unsigned int base1 = 0x00000000u;
    volatile unsigned int src1  = 0x0000000Au;
    check("bsifi_insert_nibble_4",
          BSIFI_IMMS(base1, src1, 4, 4), 0x000000A0u);

    /*
     * Insert 0xFF (8 bits) at position 8 of 0xFFFF0000 (overwrite byte 1):
     *   result[15:8] = 0xFF  →  0xFFFFFFFF
     */
    volatile unsigned int base2 = 0xFFFF0000u;
    volatile unsigned int src2  = 0x000000FFu;
    check("bsifi_insert_byte_8",
          BSIFI_IMMS(base2, src2, 8, 8), 0xFFFFFF00u);

    /* ── Summary ─────────────────────────────────────────────────────────── */
    if (g_fail == 0)
        uart_puts("PASS bsefi/bsifi ok\n");
    else
        uart_puts("FAIL see above\n");

    /* Halt here rather than returning to crt0.
     * On hardware: spin until reset.
     * On QEMU: killed by the timeout in the calling script; use -icount 0
     * so the timeout corresponds to actual instruction count, not wall clock. */
    while (1) { __asm__ volatile("" ::: "memory"); }

    return g_fail; /* unreachable */
}
