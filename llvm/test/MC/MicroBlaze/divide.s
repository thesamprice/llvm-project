# RUN: llvm-mc -triple=microblazeel -show-encoding -mattr=+divide < %s | FileCheck %s
# REQUIRES: microblaze-registered-target

# MicroBlaze hardware integer divide instruction encoding tests (UG984 §5 Fig 95).
# Requires C_USE_DIV=1 (+divide).
# Latency: 1 cycle if rA=0; 34/35/30 cycles otherwise (C_AREA_OPTIMIZED=0/1/2).
#
# Type A format: [31:26]=0x12(010010)  [25:21]=rD  [20:16]=rA  [15:11]=rB  [10:0]=func
#   idiv:  func=0x000 (U=0, signed)
#   idivu: func=0x002 (U=1 at std bit[1], unsigned)
#
# Result: rD = rB / rA. Sets MSR[DZO] on divide-by-zero or overflow.

#------------------------------------------------------------------------------
# idiv rD, rA, rB  — signed integer divide: rD = rB / rA  (func=0x000)
#
# idiv r3, r5, r7  — rD=r3=3, rA=r5=5, rB=r7=7
# Word = 0x12<<26 | 3<<21 | 5<<16 | 7<<11 | 0x000
#      = 0x48000000 | 0x00600000 | 0x00050000 | 0x00003800
#      = 0x48653800
# LE: 00 38 65 48
#------------------------------------------------------------------------------
# CHECK: idiv r3, r5, r7              # encoding: [0x00,0x38,0x65,0x48]
  idiv r3, r5, r7

# idiv r3, r5, r0  — divide r0 by r5 (always 0); word = 0x48650000 → LE: 00 00 65 48
# CHECK: idiv r3, r5, r0              # encoding: [0x00,0x00,0x65,0x48]
  idiv r3, r5, r0

#------------------------------------------------------------------------------
# idivu rD, rA, rB  — unsigned integer divide: rD = rB / rA  (func=0x002)
# U=1 at std bit[1]: word differs from idiv by 0x002.
#
# idivu r3, r5, r7  — word = 0x48653802 → LE: 02 38 65 48
#------------------------------------------------------------------------------
# CHECK: idivu r3, r5, r7             # encoding: [0x02,0x38,0x65,0x48]
  idivu r3, r5, r7

# idivu r3, r5, r0  — unsigned divide r0 by r5; word = 0x48650002 → LE: 02 00 65 48
# CHECK: idivu r3, r5, r0             # encoding: [0x02,0x00,0x65,0x48]
  idivu r3, r5, r0
