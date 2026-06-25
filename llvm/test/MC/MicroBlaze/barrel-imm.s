# RUN: llvm-mc -triple=microblazeel -show-encoding -mattr=+barrel-shift < %s | FileCheck %s
# REQUIRES: microblaze-registered-target

# MicroBlaze barrel-shift-immediate instruction encoding tests (UG984 §5 Fig 82).
# All require C_USE_BARREL=1 (+barrel-shift). Latency: 1 or 2 cycles.
# NOTE: These are NOT Type B instructions — a preceding imm prefix has no effect.
#
# Encoding (not standard Type A/B):
#   [31:26]=0x19(011001)  [25:21]=rD  [20:16]=rA
#   bsrli/bsrai/bslli: [15:11]=0  [10]=S  [9]=T  [8:5]=0  [4:0]=shamt5
#   bsefi/bsifi:       [15]=I  [14]=E  [13:11]=0  [10:6]=IMMW  [5]=0  [4:0]=IMMS
#
# S=1: shift left (bslli). T=1: arithmetic shift (bsrai).
# E=1: extract field. I=1: insert field.

#------------------------------------------------------------------------------
# bsrli rD, rA, shamt  — barrel shift right logical immediate  (S=0, T=0)
#
# bsrli r3, r5, 2  — rD=3, rA=5, shamt=2
#   [31:26]=011001  [25:21]=00011  [20:16]=00101  [15:11]=00000  [10:9]=00  [8:5]=0000  [4:0]=00010
# Word = 0x19<<26 | 3<<21 | 5<<16 | 2
#      = 0x64000000 | 0x00600000 | 0x00050000 | 0x00000002
#      = 0x64650002
# LE: 02 00 65 64
#------------------------------------------------------------------------------
# CHECK: bsrli r3, r5, 2              # encoding: [0x02,0x00,0x65,0x64]
  bsrli r3, r5, 2

# bsrli r3, r5, 0  — shamt=0; word = 0x64650000 → LE: 00 00 65 64
# CHECK: bsrli r3, r5, 0              # encoding: [0x00,0x00,0x65,0x64]
  bsrli r3, r5, 0

# bsrli r3, r5, 31  — shamt=31=0x1F; word = 0x6465001F → LE: 1F 00 65 64
# CHECK: bsrli r3, r5, 31             # encoding: [0x1f,0x00,0x65,0x64]
  bsrli r3, r5, 31

#------------------------------------------------------------------------------
# bsrai rD, rA, shamt  — barrel shift right arithmetic immediate  (S=0, T=1)
# T=1 → bit[9]=1 adds 0x200 to the encoding.
#
# bsrai r3, r5, 2  — word = 0x64650002 | 0x200 = 0x64650202
# LE: 02 02 65 64
#------------------------------------------------------------------------------
# CHECK: bsrai r3, r5, 2              # encoding: [0x02,0x02,0x65,0x64]
  bsrai r3, r5, 2

# bsrai r3, r5, 1 — word = 0x64650201 → LE: 01 02 65 64
# CHECK: bsrai r3, r5, 1              # encoding: [0x01,0x02,0x65,0x64]
  bsrai r3, r5, 1

#------------------------------------------------------------------------------
# bslli rD, rA, shamt  — barrel shift left logical immediate  (S=1, T=0)
# S=1 → bit[10]=1 adds 0x400 to the encoding.
#
# bslli r3, r5, 2  — word = 0x64650002 | 0x400 = 0x64650402
# LE: 02 04 65 64
#------------------------------------------------------------------------------
# CHECK: bslli r3, r5, 2              # encoding: [0x02,0x04,0x65,0x64]
  bslli r3, r5, 2

# bslli r3, r5, 8 — word = 0x64650408 → LE: 08 04 65 64
# CHECK: bslli r3, r5, 8              # encoding: [0x08,0x04,0x65,0x64]
  bslli r3, r5, 8

#------------------------------------------------------------------------------
# bsefi rD, rA, IMMW, IMMS  — extract bit field  (E=1, I=0)
# E=1 → bit[14]=1. IMMW in bits[10:6], IMMS in bits[4:0].
#
# bsefi r3, r5, 7, 3  — E=1, immw=7=0b00111, imms=3
#   bit[14]=1: 0x4000
#   bits[10:6]=7: 7<<6=0x1C0 → bit[8]=1,bit[7]=1,bit[6]=1
#   bits[4:0]=3: 0x03
# Word = 0x64000000 | 3<<21 | 5<<16 | 0x4000 | 0x1C0 | 3 = 0x646541C3
# LE: C3 41 65 64
#------------------------------------------------------------------------------
# CHECK: bsefi r3, r5, 7, 3           # encoding: [0xc3,0x41,0x65,0x64]
  bsefi r3, r5, 7, 3

#------------------------------------------------------------------------------
# bsifi rD, rA, IMMW, IMMS  — insert bit field  (I=1, E=0)
# I=1 → bit[15]=1: 0x8000.
#
# bsifi r3, r5, 7, 3  — I=1, immw=7, imms=3
# Word = 0x64000000 | 3<<21 | 5<<16 | 0x8000 | 0x1C0 | 3 = 0x646581C3
# LE: C3 81 65 64
#------------------------------------------------------------------------------
# CHECK: bsifi r3, r5, 7, 3           # encoding: [0xc3,0x81,0x65,0x64]
  bsifi r3, r5, 7, 3
