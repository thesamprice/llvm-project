# RUN: llvm-mc -triple=microblazeel -show-encoding < %s | FileCheck %s
# REQUIRES: microblaze-registered-target

# MicroBlaze byte-reversed load/store instruction encoding tests (UG984 §5).
# Type A instructions with func bit 9 set (0x200) enabling byte-order reversal.
#
# Type A: opcode[31:26] rD[25:21] rA[20:16] rB[15:11] func[10:0]
# func=0x200 instead of 0x000.

# lbur r3, r5, r7  — load byte unsigned, byte-reversed
# CHECK: lbur r3, r5, r7              # encoding: [0x00,0x3a,0x65,0xc0]
  lbur r3, r5, r7

# lhur r3, r5, r7  — load halfword unsigned, byte-reversed
# CHECK: lhur r3, r5, r7              # encoding: [0x00,0x3a,0x65,0xc4]
  lhur r3, r5, r7

# lwr r3, r5, r7  — load word, byte-reversed
# CHECK: lwr r3, r5, r7               # encoding: [0x00,0x3a,0x65,0xc8]
  lwr r3, r5, r7

# sbr r3, r5, r7  — store byte, byte-reversed
# CHECK: sbr r3, r5, r7               # encoding: [0x00,0x3a,0x65,0xd0]
  sbr r3, r5, r7

# shr r3, r5, r7  — store halfword, byte-reversed
# CHECK: shr r3, r5, r7               # encoding: [0x00,0x3a,0x65,0xd4]
  shr r3, r5, r7

# swr r3, r5, r7  — store word, byte-reversed
# CHECK: swr r3, r5, r7               # encoding: [0x00,0x3a,0x65,0xd8]
  swr r3, r5, r7
