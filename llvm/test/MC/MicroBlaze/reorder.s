# RUN: llvm-mc -triple=microblazeel -show-encoding -mattr=+reorder < %s | FileCheck %s
# REQUIRES: microblaze-registered-target

# MicroBlaze byte/halfword reorder instruction encoding tests (UG984 §5 Figs 137-138).
# Requires C_USE_REORDER_INSTR=1 (+reorder). Latency: 1 cycle.
#
# Type A format: [31:26]=0x24(100100)  [25:21]=rD  [20:16]=rA  [15:11]=rB(=0)  [10:0]=func
#   swapb: func=0x1E0 = 0b000_1111_0000_0 (bits[8:5]=1111)
#   swaph: func=0x1E2 = 0b000_1111_0000_10 (bits[8:5]=1111, bit[1]=1)
#
# SWAPB rD, rA — reverse byte order: rD[7:0]=rA[31:24], ..., rD[31:24]=rA[7:0]
# SWAPH rD, rA — swap halfwords:     rD[15:0]=rA[31:16], rD[31:16]=rA[15:0]

#------------------------------------------------------------------------------
# swapb rD, rA  — swap all 4 bytes in rA  (func=0x1E0)
#
# swapb r3, r5  — rD=r3=3, rA=r5=5, rB=0
#   func=0x1E0=480=0b00_1111_0000_0 → bits[8:5]=1111
# Word = 0x24<<26 | 3<<21 | 5<<16 | 0 | 0x1E0
#      = 0x90000000 | 0x00600000 | 0x00050000 | 0x000001E0
#      = 0x906501E0
# LE: E0 01 65 90
#------------------------------------------------------------------------------
# CHECK: swapb r3, r5                  # encoding: [0xe0,0x01,0x65,0x90]
  swapb r3, r5

# swapb r0, r0  — word = 0x900001E0 → LE: E0 01 00 90
# CHECK: swapb r0, r0                  # encoding: [0xe0,0x01,0x00,0x90]
  swapb r0, r0

#------------------------------------------------------------------------------
# swaph rD, rA  — swap two halfwords in rA  (func=0x1E2)
# func=0x1E2=482=0b00_1111_0000_10 → bits[8:5]=1111, bit[1]=1
#
# swaph r3, r5  — word = 0x906501E0 | 0x002 = 0x906501E2
# LE: E2 01 65 90
#------------------------------------------------------------------------------
# CHECK: swaph r3, r5                  # encoding: [0xe2,0x01,0x65,0x90]
  swaph r3, r5

# swaph r0, r0  — word = 0x900001E2 → LE: E2 01 00 90
# CHECK: swaph r0, r0                  # encoding: [0xe2,0x01,0x00,0x90]
  swaph r0, r0
