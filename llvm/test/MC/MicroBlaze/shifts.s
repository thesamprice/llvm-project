# RUN: llvm-mc -triple=microblazeel -show-encoding < %s | FileCheck %s
# REQUIRES: microblaze-registered-target

# MicroBlaze single-bit shift and sign-extend encoding tests (UG984 §5).
# All use opcode 0x24 (100100), rB=0; func field selects the operation.
#
# Type A  opcode[31:26] rD[25:21] rA[20:16] rB[15:11]=0 func[10:0]
#
# func  bin          operation
# 0x001 00000000001  SRA  — arith right shift 1, MSR.C ← bit[0]
# 0x021 00000100001  SRC  — shift right through carry
# 0x041 01000000001  SRL  — logical right shift 1
# 0x060 01100000000  SEXT8  — sign-extend byte
# 0x061 01100000001  SEXT16 — sign-extend halfword

# sra r3, r5  — opcode=0x24, rD=3, rA=5, rB=0, func=0x001
# opcode bits[31:26]=100100, rD[25:21]=00011, rA[20:16]=00101, rB[15:11]=00000, func[10:0]=00000000001
# Word: 0b100100_00011_00101_00000_00000000001 = 0x90650001
# LE bytes: 01 00 65 90
# CHECK: sra r3, r5                 # encoding: [0x01,0x00,0x65,0x90]
  sra r3, r5

# src r3, r5  — func=0x021 = 0b00000100001
# Word bits[10:0]: 00000100001
# byte1 = {rB[4:0]=00000, func[10:8]=000} = 0x00
# byte0 = func[7:0] = 0b00100001 = 0x21
# Word: 0x90650021 → LE: 21 00 65 90
# CHECK: src r3, r5                 # encoding: [0x21,0x00,0x65,0x90]
  src r3, r5

# srl r3, r5  — func=0x041 = 0b01000000001
# byte1 = {00000, func[10:8]=010} = 0b00000010 = 0x02
# byte0 = func[7:0] = 0b00000001 = 0x01

# Wait: func=0x041=65: bits 10..0 = 0,1,0,0,0,0,0,0,0,0,1
# bit10=0, bit9=1, bit8=0, bit7=0..bit1=0, bit0=1
# byte1[15:8] = {rB[4:0]=0,func[10:8]=010} = 0b00000010 = 0x02
# byte0[7:0]  = func[7:0] = 0b00000001 = 0x01
# Word: 0x90650201 → LE: 01 02 65 90
# CHECK: srl r3, r5                 # encoding: [0x01,0x02,0x65,0x90]
  srl r3, r5

# sext8 r3, r5  — func=0x060 = 0b01100000000
# bit10=0, bit9=1, bit8=1, bit7..bit0=0
# byte1 = {00000, 011} = 0b00000011 = 0x03
# byte0 = 0b00000000 = 0x00
# Word: 0x90650300 → LE: 00 03 65 90
# CHECK: sext8 r3, r5              # encoding: [0x00,0x03,0x65,0x90]
  sext8 r3, r5

# sext16 r3, r5  — func=0x061 = 0b01100000001
# byte1 = {00000, 011} = 0x03
# byte0 = 0b00000001 = 0x01
# Word: 0x90650301 → LE: 01 03 65 90
# CHECK: sext16 r3, r5             # encoding: [0x01,0x03,0x65,0x90]
  sext16 r3, r5
