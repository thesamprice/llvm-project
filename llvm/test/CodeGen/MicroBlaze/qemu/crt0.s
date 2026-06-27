// Minimal bare-metal startup for petalogix-s3adsp1800 (MicroBlaze LE).
// Sets the stack pointer to the top of the 128 MB RAM region, then
// calls main().  The return value of main() is ignored; we halt.
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
        nop
halt:
        bri     halt
        nop
