# REQUIRES: microblaze
# RUN: llvm-mc -filetype=obj -triple=microblazeel %s -o %t.o
# RUN: printf '.global _start\n.text\n_start:\nnop\n' | \
# RUN:   llvm-mc -filetype=obj -triple=microblazeel -o %t2.o -
# RUN: ld.lld -o %t %t.o %t2.o --Ttext=0x3000 --image-base=0x0
# RUN: llvm-objdump -s %t | FileCheck %s

## R_MICROBLAZE_64_PCREL: IMM+branch pair with PC-relative target.
##
## Layout after linking (--Ttext=0x3000, beqid before _start):
##   0x3000: IMM instruction (hi16 of offset)
##   0x3004: BEQID r4 (lo16 of offset)
##   0x3008: _start nop
##
## val    = S + A - P = 0x3008 + 0 - 0x3000 = 0x8  (P = IMM address)
## offset = val - 4  = 0x4                          (branch PC = IMM + 4)
##
## IMM  word 0xB0000000 | (4 >> 16)          = 0xB0000000 → LE: 00 00 00 b0
## BEQID word 0xBE040000 | (4 & 0xFFFF)      = 0xBE040004 → LE: 04 00 04 be

.text
beqid r4, _start    ## R_MICROBLAZE_64_PCREL

# CHECK: Contents of section .text:
# CHECK: 3000 000000b0 040004be 000000{{..}}
