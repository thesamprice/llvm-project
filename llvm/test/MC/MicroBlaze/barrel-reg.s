# RUN: llvm-mc -triple=microblazeel -show-encoding -mattr=+barrel-shift < %s | FileCheck %s
# REQUIRES: microblaze-registered-target

# MicroBlaze register barrel-shift instruction encoding tests (UG984 §5).
# All are Type A: opcode=0x11 (010001), func field selects direction.
# Type A: opcode[31:26] rD[25:21] rA[20:16] rB[15:11] func[10:0]
#
# Using rD=r3, rA=r5, rB=r7 throughout.
# Word = 0x44000000 | (3<<21) | (5<<16) | (7<<11) | func

# bsrl r3, r5, r7  — barrel shift right logical; func=0x000
# Word = 0x44653800  LE: [0x00,0x38,0x65,0x44]
# CHECK: bsrl r3, r5, r7              # encoding: [0x00,0x38,0x65,0x44]
  bsrl r3, r5, r7

# bsra r3, r5, r7  — barrel shift right arithmetic; func=0x200
# Word = 0x44653A00  LE: [0x00,0x3a,0x65,0x44]
# CHECK: bsra r3, r5, r7              # encoding: [0x00,0x3a,0x65,0x44]
  bsra r3, r5, r7

# bsll r3, r5, r7  — barrel shift left logical; func=0x400
# Word = 0x44653C00  LE: [0x00,0x3c,0x65,0x44]
# CHECK: bsll r3, r5, r7              # encoding: [0x00,0x3c,0x65,0x44]
  bsll r3, r5, r7
