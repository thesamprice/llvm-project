# RUN: llvm-mc -triple=microblazeel -show-encoding < %s | FileCheck %s
# REQUIRES: microblaze-registered-target

# MicroBlaze pattern-compare and CLZ instruction encoding tests (UG984 §5).
# All require C_USE_PCMP_INSTR=1. Latency: 1 cycle each.
#
# Type A format: [31:26]=opcode  [25:21]=rD  [20:16]=rA  [15:11]=rB  [10:0]=func
#
# pcmpbf (Fig 116): opcode=0x20 (100000),  func=0x400 (10000000000)
# pcmpeq (Fig 117): opcode=0x22 (100010),  func=0x400
# pcmpne (Fig 118): opcode=0x23 (100011),  func=0x400
# clz    (Fig 83):  opcode=0x24 (100100),  func=0x0E0 (00011100000), rB=0

#------------------------------------------------------------------------------
# pcmpbf rD, rA, rB  — bytewise compare; rD = position of first matching byte
#
# pcmpbf r3, r5, r7  — rD=r3=3, rA=r5=5, rB=r7=7
#   [31:26]=100000  [25:21]=00011  [20:16]=00101  [15:11]=00111  [10:0]=10000000000
# Word = 0x20<<26 | 3<<21 | 5<<16 | 7<<11 | 0x400
#      = 0x80000000 | 0x00600000 | 0x00050000 | 0x00003800 | 0x00000400
#      = 0x80653C00
# LE: 00 3C 65 80
#------------------------------------------------------------------------------
# CHECK: pcmpbf r3, r5, r7             # encoding: [0x00,0x3c,0x65,0x80]
  pcmpbf r3, r5, r7

# pcmpbf r0, r0, r0  — all-zero registers; word = 0x80000400 → LE: 00 04 00 80
# CHECK: pcmpbf r0, r0, r0             # encoding: [0x00,0x04,0x00,0x80]
  pcmpbf r0, r0, r0

#------------------------------------------------------------------------------
# pcmpeq rD, rA, rB  — equality compare; rD = 1 if rA == rB, else 0
#
# pcmpeq r3, r5, r7  — rD=3, rA=5, rB=7
# Word = 0x22<<26 | 3<<21 | 5<<16 | 7<<11 | 0x400
#      = 0x88000000 | 0x00600000 | 0x00050000 | 0x00003800 | 0x00000400
#      = 0x88653C00
# LE: 00 3C 65 88
#------------------------------------------------------------------------------
# CHECK: pcmpeq r3, r5, r7             # encoding: [0x00,0x3c,0x65,0x88]
  pcmpeq r3, r5, r7

#------------------------------------------------------------------------------
# pcmpne rD, rA, rB  — not-equal compare; rD = 1 if rA != rB, else 0
#
# pcmpne r3, r5, r7  — rD=3, rA=5, rB=7
# Word = 0x23<<26 | 3<<21 | 5<<16 | 7<<11 | 0x400
#      = 0x8C000000 | 0x00600000 | 0x00050000 | 0x00003800 | 0x00000400
#      = 0x8C653C00
# LE: 00 3C 65 8C
#------------------------------------------------------------------------------
# CHECK: pcmpne r3, r5, r7             # encoding: [0x00,0x3c,0x65,0x8c]
  pcmpne r3, r5, r7

#------------------------------------------------------------------------------
# clz rD, rA  — count leading zeros; rD = clz(rA); rB field fixed to 0
#
# clz r3, r5  — rD=3, rA=5, rB=0
#   [31:26]=100100  [25:21]=00011  [20:16]=00101  [15:11]=00000  [10:0]=00011100000
# Word = 0x24<<26 | 3<<21 | 5<<16 | 0 | 0x0E0
#      = 0x90000000 | 0x00600000 | 0x00050000 | 0x000000E0
#      = 0x906500E0
# LE: E0 00 65 90
#------------------------------------------------------------------------------
# CHECK: clz r3, r5                    # encoding: [0xe0,0x00,0x65,0x90]
  clz r3, r5

# clz r0, r0  — all-zero; word = 0x900000E0 → LE: E0 00 00 90
# CHECK: clz r0, r0                    # encoding: [0xe0,0x00,0x00,0x90]
  clz r0, r0
