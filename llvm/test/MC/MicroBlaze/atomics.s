# RUN: llvm-mc -triple=microblazeel -show-encoding < %s | FileCheck %s
# REQUIRES: microblaze-registered-target

# MicroBlaze exclusive load/store instruction encoding tests (UG984 §5 Figs 103, 140).
# LWX/SWX are base ISA instructions; no feature flag required.
# Latency: 1 cycle (C_AREA_OPTIMIZED=0 or 2), 2 cycles (C_AREA_OPTIMIZED=1).
#
# Type A format: [31:26]=opcode  [25:21]=rD  [20:16]=rA  [15:11]=rB  [10:0]=func
#   lwx: opcode=0x32 (110010), func=0x400 (bit[10]=1)  — shares opcode with lw (func=0)
#   swx: opcode=0x36 (110110), func=0x400              — shares opcode with sw (func=0)

#------------------------------------------------------------------------------
# lwx rD, rA, rB  — load word exclusive: rD = Mem[rA+rB]; set reservation
#
# lwx r3, r5, r7  — rD=3, rA=5, rB=7
# Word = 0x32<<26 | 3<<21 | 5<<16 | 7<<11 | 0x400
#      = 0xC8000000 | 0x00600000 | 0x00050000 | 0x00003800 | 0x00000400
#      = 0xC8653C00
# LE: 00 3C 65 C8
#------------------------------------------------------------------------------
# CHECK: lwx r3, r5, r7               # encoding: [0x00,0x3c,0x65,0xc8]
  lwx r3, r5, r7

# lwx r3, r0, r0  — load from address 0; word = 0xC8600400 → LE: 00 04 60 C8
# Word = 0xC8000000 | 3<<21 | 0 | 0 | 0x400 = 0xC8600400
# LE: 00 04 60 C8
# CHECK: lwx r3, r0, r0               # encoding: [0x00,0x04,0x60,0xc8]
  lwx r3, r0, r0

#------------------------------------------------------------------------------
# swx rD, rA, rB  — store word exclusive: Mem[rA+rB] = rD (conditional)
# MSR[C]=0 on success, MSR[C]=1 if store suppressed; reservation always cleared.
#
# swx r3, r5, r7  — rD=3, rA=5, rB=7
# Word = 0x36<<26 | 3<<21 | 5<<16 | 7<<11 | 0x400
#      = 0xD8000000 | 0x00600000 | 0x00050000 | 0x00003800 | 0x00000400
#      = 0xD8653C00
# LE: 00 3C 65 D8
#------------------------------------------------------------------------------
# CHECK: swx r3, r5, r7               # encoding: [0x00,0x3c,0x65,0xd8]
  swx r3, r5, r7

# swx r3, r0, r0  — store to address 0; word = 0xD8600400 → LE: 00 04 60 D8
# CHECK: swx r3, r0, r0               # encoding: [0x00,0x04,0x60,0xd8]
  swx r3, r0, r0
