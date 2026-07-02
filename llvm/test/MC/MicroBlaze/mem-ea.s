# RUN: llvm-mc -triple=microblazeel -show-encoding -mattr=+extended-addr < %s | FileCheck %s
# REQUIRES: microblaze-registered-target

# MicroBlaze extended-address (EA) load/store encoding tests (UG984 §5).
# MB-X 64-bit addressing: same opcodes as normal loads/stores with func bit 7
# set (0x080). Gated on the +extended-addr subtarget feature.
#
# Type A: opcode[31:26] rD[25:21] rA[20:16] rB[15:11] func[10:0]
# func=0x080 for EA variants vs func=0x000 for normal register-indexed.

# lbuea r3, r5, r7  — load byte unsigned, extended address
# opcode=0x30, func=0x080
# Word = 0xC0000000 | (3<<21) | (5<<16) | (7<<11) | 0x080 → 0xC0653880
# LE: [0x80,0x38,0x65,0xc0]
# CHECK: lbuea r3, r5, r7             # encoding: [0x80,0x38,0x65,0xc0]
  lbuea r3, r5, r7

# lhuea r3, r5, r7  — load halfword unsigned, extended address
# opcode=0x31 → base 0xC4000000
# CHECK: lhuea r3, r5, r7             # encoding: [0x80,0x38,0x65,0xc4]
  lhuea r3, r5, r7

# lwea r3, r5, r7  — load word, extended address
# opcode=0x32 → base 0xC8000000
# CHECK: lwea r3, r5, r7              # encoding: [0x80,0x38,0x65,0xc8]
  lwea r3, r5, r7

# sbea r3, r5, r7  — store byte, extended address
# opcode=0x34 → base 0xD0000000
# CHECK: sbea r3, r5, r7              # encoding: [0x80,0x38,0x65,0xd0]
  sbea r3, r5, r7

# shea r3, r5, r7  — store halfword, extended address
# opcode=0x35 → base 0xD4000000
# CHECK: shea r3, r5, r7              # encoding: [0x80,0x38,0x65,0xd4]
  shea r3, r5, r7

# swea r3, r5, r7  — store word, extended address
# opcode=0x36 → base 0xD8000000
# CHECK: swea r3, r5, r7              # encoding: [0x80,0x38,0x65,0xd8]
  swea r3, r5, r7
