# RUN: llvm-mc -triple=microblazeel -show-encoding < %s | FileCheck %s
# REQUIRES: microblaze-registered-target

# MicroBlaze register-indexed load/store encoding tests (UG984 §5).
# Type A: opcode[31:26] rD[25:21] rA[20:16] rB[15:11] func[10:0]
# func=0x000 for normal register-indexed (vs 0x080=EA, 0x200=byte-reversed).
#
# Using rD=r3, rA=r5, rB=r7 throughout.
# Word = opcode<<26 | (3<<21) | (5<<16) | (7<<11) | 0x000

# lbu r3, r5, r7  — load byte unsigned; opcode=0x30
# Word = 0xC0653800  LE: [0x00,0x38,0x65,0xc0]
# CHECK: lbu r3, r5, r7               # encoding: [0x00,0x38,0x65,0xc0]
  lbu r3, r5, r7

# lhu r3, r5, r7  — load halfword unsigned; opcode=0x31
# Word = 0xC4653800  LE: [0x00,0x38,0x65,0xc4]
# CHECK: lhu r3, r5, r7               # encoding: [0x00,0x38,0x65,0xc4]
  lhu r3, r5, r7

# lw r3, r5, r7  — load word; opcode=0x32
# Word = 0xC8653800  LE: [0x00,0x38,0x65,0xc8]
# CHECK: lw r3, r5, r7                # encoding: [0x00,0x38,0x65,0xc8]
  lw r3, r5, r7

# sb r3, r5, r7  — store byte; opcode=0x34
# Word = 0xD0653800  LE: [0x00,0x38,0x65,0xd0]
# CHECK: sb r3, r5, r7                # encoding: [0x00,0x38,0x65,0xd0]
  sb r3, r5, r7

# sh r3, r5, r7  — store halfword; opcode=0x35
# Word = 0xD4653800  LE: [0x00,0x38,0x65,0xd4]
# CHECK: sh r3, r5, r7                # encoding: [0x00,0x38,0x65,0xd4]
  sh r3, r5, r7

# sw r3, r5, r7  — store word; opcode=0x36
# Word = 0xD8653800  LE: [0x00,0x38,0x65,0xd8]
# CHECK: sw r3, r5, r7                # encoding: [0x00,0x38,0x65,0xd8]
  sw r3, r5, r7
