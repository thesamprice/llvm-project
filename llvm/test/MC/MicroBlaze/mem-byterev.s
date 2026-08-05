# RUN: llvm-mc -triple=microblazeel -show-encoding < %s | FileCheck %s
# REQUIRES: microblaze-registered-target

# MicroBlaze byte-reversed load/store instruction encoding tests (UG984 §5).
# These are Type A instructions with func bit 9 set (0x200) enabling
# byte-order reversal as the data passes through the memory interface.
#
# Type A: opcode[31:26] rD[25:21] rA[20:16] rB[15:11] func[10:0]
# Same opcodes as normal loads/stores (LBU=0x30, LHU=0x31, LW=0x32,
# SB=0x34, SH=0x35, SW=0x36) with func=0x200 instead of 0x000.

# lbur r3, r5, r7  — load byte unsigned, byte-reversed
# opcode=0x30, rD=3, rA=5, rB=7, func=0x200
# Word = 0xC0000000 | (3<<21) | (5<<16) | (7<<11) | 0x200
#      = 0xC0653A00  → LE: [0x00,0x3a,0x65,0xc0]
# CHECK: lbur r3, r5, r7              # encoding: [0x00,0x3a,0x65,0xc0]
  lbur r3, r5, r7

# lhur r3, r5, r7  — load halfword unsigned, byte-reversed
# opcode=0x31 → base 0xC4000000
# CHECK: lhur r3, r5, r7              # encoding: [0x00,0x3a,0x65,0xc4]
  lhur r3, r5, r7

# lwr r3, r5, r7  — load word, byte-reversed
# opcode=0x32 → base 0xC8000000
# CHECK: lwr r3, r5, r7               # encoding: [0x00,0x3a,0x65,0xc8]
  lwr r3, r5, r7

# sbr r3, r5, r7  — store byte, byte-reversed
# opcode=0x34 → base 0xD0000000
# CHECK: sbr r3, r5, r7               # encoding: [0x00,0x3a,0x65,0xd0]
  sbr r3, r5, r7

# shr r3, r5, r7  — store halfword, byte-reversed
# opcode=0x35 → base 0xD4000000
# CHECK: shr r3, r5, r7               # encoding: [0x00,0x3a,0x65,0xd4]
  shr r3, r5, r7

# swr r3, r5, r7  — store word, byte-reversed
# opcode=0x36 → base 0xD8000000
# CHECK: swr r3, r5, r7               # encoding: [0x00,0x3a,0x65,0xd8]
  swr r3, r5, r7
