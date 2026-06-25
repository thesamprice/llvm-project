# RUN: llvm-mc -triple=microblazeel -show-encoding -mattr=+hard-float < %s | FileCheck %s
# REQUIRES: microblaze-registered-target

# MicroBlaze hardware floating-point instruction encoding tests (UG984 §5 Figs 83-89).
# Requires C_USE_FPU >= 1 (+hard-float). All FPU instructions use opcode 0x16 (010110).
#
# Type A format: [31:26]=0x16  [25:21]=rD  [20:16]=rA  [15:11]=rB  [10:0]=func
#
#   fadd  func=0x000   frsub func=0x080   fmul  func=0x100   fdiv  func=0x180
#   fcmp.un func=0x200  OpSel=0b000
#   fcmp.lt func=0x210  OpSel=0b001  (bits[6:4] = OpSel)
#   fcmp.eq func=0x220  OpSel=0b010
#   fcmp.le func=0x230  OpSel=0b011
#   fcmp.gt func=0x240  OpSel=0b100
#   fcmp.ne func=0x250  OpSel=0b101
#   fcmp.ge func=0x260  OpSel=0b110
#
# For rD=r3=3, rA=r5=5, rB=r7=7:
#   base word = 0x16<<26 | 3<<21 | 5<<16 | 7<<11 = 0x58653800

#------------------------------------------------------------------------------
# fadd rD, rA, rB — float add  (func=0x000)
# Word = 0x58653800 | 0x000 = 0x58653800
# LE: 00 38 65 58
#------------------------------------------------------------------------------
# CHECK: fadd r3, r5, r7              # encoding: [0x00,0x38,0x65,0x58]
  fadd r3, r5, r7

#------------------------------------------------------------------------------
# frsub rD, rA, rB — float reverse-subtract rD = rB - rA  (func=0x080)
# Word = 0x58653800 | 0x080 = 0x58653880
# LE: 80 38 65 58
#------------------------------------------------------------------------------
# CHECK: frsub r3, r5, r7             # encoding: [0x80,0x38,0x65,0x58]
  frsub r3, r5, r7

#------------------------------------------------------------------------------
# fmul rD, rA, rB — float multiply  (func=0x100)
# Word = 0x58653800 | 0x100 = 0x58653900
# LE: 00 39 65 58
#------------------------------------------------------------------------------
# CHECK: fmul r3, r5, r7              # encoding: [0x00,0x39,0x65,0x58]
  fmul r3, r5, r7

#------------------------------------------------------------------------------
# fdiv rD, rA, rB — float divide  (func=0x180)
# Word = 0x58653800 | 0x180 = 0x58653980
# LE: 80 39 65 58
#------------------------------------------------------------------------------
# CHECK: fdiv r3, r5, r7              # encoding: [0x80,0x39,0x65,0x58]
  fdiv r3, r5, r7

#------------------------------------------------------------------------------
# fcmp.un rD, rA, rB — float compare unordered  (func=0x200, OpSel=0b000)
# Word = 0x58653800 | 0x200 = 0x58653A00
# LE: 00 3a 65 58
#------------------------------------------------------------------------------
# CHECK: fcmp.un r3, r5, r7           # encoding: [0x00,0x3a,0x65,0x58]
  fcmp.un r3, r5, r7

#------------------------------------------------------------------------------
# fcmp.lt rD, rA, rB — float compare less-than  (func=0x210, OpSel=0b001)
# OpSel=001 → bits[6:4]=001 → 0x010; func = 0x200|0x010 = 0x210
# Word = 0x58653800 | 0x210 = 0x58653A10
# LE: 10 3a 65 58
#------------------------------------------------------------------------------
# CHECK: fcmp.lt r3, r5, r7           # encoding: [0x10,0x3a,0x65,0x58]
  fcmp.lt r3, r5, r7

#------------------------------------------------------------------------------
# fcmp.eq rD, rA, rB — float compare equal  (func=0x220, OpSel=0b010)
# func = 0x200|0x020 = 0x220 → LE: 20 3a 65 58
#------------------------------------------------------------------------------
# CHECK: fcmp.eq r3, r5, r7           # encoding: [0x20,0x3a,0x65,0x58]
  fcmp.eq r3, r5, r7

#------------------------------------------------------------------------------
# fcmp.le rD, rA, rB — float compare less-or-equal  (func=0x230, OpSel=0b011)
# func = 0x200|0x030 = 0x230 → LE: 30 3a 65 58
#------------------------------------------------------------------------------
# CHECK: fcmp.le r3, r5, r7           # encoding: [0x30,0x3a,0x65,0x58]
  fcmp.le r3, r5, r7

#------------------------------------------------------------------------------
# fcmp.gt rD, rA, rB — float compare greater-than  (func=0x240, OpSel=0b100)
# func = 0x200|0x040 = 0x240 → LE: 40 3a 65 58
#------------------------------------------------------------------------------
# CHECK: fcmp.gt r3, r5, r7           # encoding: [0x40,0x3a,0x65,0x58]
  fcmp.gt r3, r5, r7

#------------------------------------------------------------------------------
# fcmp.ne rD, rA, rB — float compare not-equal  (func=0x250, OpSel=0b101)
# func = 0x200|0x050 = 0x250 → LE: 50 3a 65 58
#------------------------------------------------------------------------------
# CHECK: fcmp.ne r3, r5, r7           # encoding: [0x50,0x3a,0x65,0x58]
  fcmp.ne r3, r5, r7

#------------------------------------------------------------------------------
# fcmp.ge rD, rA, rB — float compare greater-or-equal  (func=0x260, OpSel=0b110)
# func = 0x200|0x060 = 0x260 → LE: 60 3a 65 58
#------------------------------------------------------------------------------
# CHECK: fcmp.ge r3, r5, r7           # encoding: [0x60,0x3a,0x65,0x58]
  fcmp.ge r3, r5, r7
