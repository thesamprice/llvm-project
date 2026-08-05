// Minimal bare-metal startup for petalogix-s3adsp1800 (MicroBlaze LE).
// Sets the stack pointer to the top of the 128 MB RAM region, calls
// main(), then writes 0 to the exit device at 0xFF000000 for a clean
// QEMU shutdown (no halt-loop polling needed).
//
// RAM layout: 0x90000000 base, 128 MB (0x08000000 bytes).
// Stack top  = 0x90000000 + 0x08000000 - 16 = 0x97FFFFF0
// Encoded as: imm 0x97FF ; addik r1, r0, -16
//
        .section .text.start,"ax"
        .global  _start
_start:
        imm     0x97FF
        addik   r1, r0, -16         // r1 = 0x97FFFFF0 (stack top)
        bralid  r15, main
        or      r0, r0, r0          // delay slot (NOP)
        // Shut down QEMU cleanly: write 0 to exit device.
        imm     0xFF00
        addik   r4, r0, 0x0000      // r4 = 0xFF000000
        swi     r0, r4, 0           // write 0 → clean exit
