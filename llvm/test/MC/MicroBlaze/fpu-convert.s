# RUN: llvm-mc -triple=microblazeel -show-encoding -mattr=+float-convert < %s | FileCheck %s
# REQUIRES: microblaze-registered-target

# MicroBlaze floating-point conversion instruction encoding tests (UG984 §5 Figs 90-92).
# Requires C_USE_FPU=2 (+float-convert). Opcode 0x16 (010110), rB=0 (fixed).
#
# Type A format: [31:26]=0x16  [25:21]=rD  [20:16]=rA  [15:11]=0  [10:0]=func
#   flt   rD, rA — signed int → float  func=0x280  (5/7/2 cycles area opt 0/1/2)
#   fint  rD, rA — float → signed int  func=0x300  (4/6/1 cycles)
#   fsqrt rD, rA — float square root   func=0x380  (27/29/23 cycles)
#
# For rD=r3=3, rA=r5=5, rB=0 (fixed):
#   base word (no rB) = 0x16<<26 | 3<<21 | 5<<16 | 0<<11 = 0x58650000

#------------------------------------------------------------------------------
# flt rD, rA — convert signed integer in rA to float  (func=0x280)
# Word = 0x58650000 | 0x280 = 0x58650280
# LE: 80 02 65 58
#------------------------------------------------------------------------------
# CHECK: flt r3, r5                   # encoding: [0x80,0x02,0x65,0x58]
  flt r3, r5

#------------------------------------------------------------------------------
# fint rD, rA — convert float in rA to signed integer (truncate)  (func=0x300)
# Word = 0x58650000 | 0x300 = 0x58650300
# LE: 00 03 65 58
#------------------------------------------------------------------------------
# CHECK: fint r3, r5                  # encoding: [0x00,0x03,0x65,0x58]
  fint r3, r5

#------------------------------------------------------------------------------
# fsqrt rD, rA — floating-point square root of rA  (func=0x380)
# Word = 0x58650000 | 0x380 = 0x58650380
# LE: 80 03 65 58
#------------------------------------------------------------------------------
# CHECK: fsqrt r3, r5                 # encoding: [0x80,0x03,0x65,0x58]
  fsqrt r3, r5
