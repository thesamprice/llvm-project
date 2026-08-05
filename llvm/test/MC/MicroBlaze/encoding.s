# RUN: llvm-mc -triple=microblazeel -show-encoding < %s | FileCheck %s
# REQUIRES: microblaze-registered-target

# MicroBlaze instruction encoding tests (UG984 Ch.5).
# Encoding is 32-bit big-endian bit-numbered, stored LE in memory.

#------------------------------------------------------------------------------
# Type A format: opcode[31:26] rD[25:21] rA[20:16] rB[15:11] func[10:0]
#
# All bytes shown little-endian (byte 0 first = low address).
#------------------------------------------------------------------------------

# addk r3, r4, r5  — opcode=0x04, rD=3, rA=4, rB=5, func=0
# Word: 0b000100_00011_00100_00101_00000000000 = 0x10642800
# LE bytes: 00 28 64 10
# CHECK: addk r3, r4, r5             # encoding: [0x00,0x28,0x64,0x10]
  addk r3, r4, r5

# or r0, r0, r0  — NOP, opcode=0x20, rD=0, rA=0, rB=0, func=0
# Word: 0b100000_00000_00000_00000_00000000000 = 0x80000000
# LE bytes: 00 00 00 80
# CHECK: or r0, r0, r0               # encoding: [0x00,0x00,0x00,0x80]
  or r0, r0, r0

# and r3, r4, r5  — opcode=0x21, rD=3, rA=4, rB=5, func=0
# Word: 0b100001_00011_00100_00101_00000000000 = 0x84642800
# LE bytes: 00 28 64 84
# CHECK: and r3, r4, r5              # encoding: [0x00,0x28,0x64,0x84]
  and r3, r4, r5

# xor r3, r4, r5  — opcode=0x22
# Word: 0b100010_00011_00100_00101_00000000000 = 0x88642800
# LE bytes: 00 28 64 88
# CHECK: xor r3, r4, r5              # encoding: [0x00,0x28,0x64,0x88]
  xor r3, r4, r5

# andn r3, r4, r5  — opcode=0x23
# Word: 0b100011_00011_00100_00101_00000000000 = 0x8C642800
# LE bytes: 00 28 64 8C
# CHECK: andn r3, r4, r5             # encoding: [0x00,0x28,0x64,0x8c]
  andn r3, r4, r5

# mul r3, r4, r5  — opcode=0x10, func=0
# Word: 0b010000_00011_00100_00101_00000000000 = 0x40642800
# LE bytes: 00 28 64 40
# CHECK: mul r3, r4, r5              # encoding: [0x00,0x28,0x64,0x40]
  mul r3, r4, r5

#------------------------------------------------------------------------------
# Type B format: opcode[31:26] rD[25:21] rA[20:16] imm16[15:0]
#
# imm16 is in bits[15:0]; in LE layout those are bytes 0-1.
#------------------------------------------------------------------------------

# addik r3, r4, 1  — opcode=0x0C, rD=3, rA=4, imm=1
# Word: 0b001100_00011_00100_0000000000000001 = 0x30640001
# LE bytes: 01 00 64 30
# CHECK: addik r3, r4, 1             # encoding: [0x01,0x00,0x64,0x30]
  addik r3, r4, 1

# ori r3, r4, 42  — opcode=0x28, rD=3, rA=4, imm=42=0x2A
# Word: 0b101000_00011_00100_0000000000101010 = 0xA064002A
# LE bytes: 2A 00 64 A0
# CHECK: ori r3, r4, 42              # encoding: [0x2a,0x00,0x64,0xa0]
  ori r3, r4, 42

# andi r3, r4, 255  — opcode=0x29, imm=0xFF
# Word: 0b101001_00011_00100_0000000011111111 = 0xA46400FF
# LE bytes: FF 00 64 A4
# CHECK: andi r3, r4, 255            # encoding: [0xff,0x00,0x64,0xa4]
  andi r3, r4, 255

# muli r3, r4, 10  — opcode=0x18, imm=10
# Word: 0b011000_00011_00100_0000000000001010 = 0x6064000A
# LE bytes: 0A 00 64 60
# CHECK: muli r3, r4, 10             # encoding: [0x0a,0x00,0x64,0x60]
  muli r3, r4, 10

# rtsd r15, 8  — opcode=0x2D, rD=0b10000 (subroutine return type), rA=r15, imm=8
# Per UG984, bits[25:21] encode the return type: 0b10000 for rtsd.
# Word: 0b101101_10000_01111_0000000000001000 = 0xB60F0008
# LE bytes: 08 00 0F B6
# CHECK: rtsd r15, 8                 # encoding: [0x08,0x00,0x0f,0xb6]
  rtsd r15, 8

# lwi r3, r1, 0  — opcode=0x3A, rD=3, rA=1, imm=0
# Word: 0b111010_00011_00001_0000000000000000 = 0xE8610000
# LE bytes: 00 00 61 E8
# CHECK: lwi r3, r1, 0               # encoding: [0x00,0x00,0x61,0xe8]
  lwi r3, r1, 0

# swi r3, r1, 4  — opcode=0x3E, rD=3, rA=1, imm=4
# Word: 0b111110_00011_00001_0000000000000100 = 0xF8610004
# LE bytes: 04 00 61 F8
# CHECK: swi r3, r1, 4               # encoding: [0x04,0x00,0x61,0xf8]
  swi r3, r1, 4

#------------------------------------------------------------------------------
# Conditional branches — rD encodes delay-bit and condition (UG984 Ch.5)
#   beqid: opcode=0x2F, rD=0b10000 (EQ + delay)
#   bneid: opcode=0x2F, rD=0b10001 (NE + delay)
#   bltid: opcode=0x2F, rD=0b10010 (LT + delay)
#------------------------------------------------------------------------------

# beqid r4, .+0  — imm=0
# rD=0b10000=16, opcode=0x2F
# Word: 0b101111_10000_00100_0000000000000000 = 0xBE040000
# LE bytes: 00 00 04 BE
# CHECK: beqid r4, 0                 # encoding: [0x00,0x00,0x04,0xbe]
  beqid r4, 0

# bneid r4, .+0
# rD=0b10001=17
# Word: 0b101111_10001_00100_0000000000000000 = 0xBE240000
# LE bytes: 00 00 24 BE
# CHECK: bneid r4, 0                 # encoding: [0x00,0x00,0x24,0xbe]
  bneid r4, 0

# bltid r4, 0 — rD=0b10010=18 (LT + delay)
# Word: 0xBE440000  LE: 00 00 44 BE
# CHECK: bltid r4, 0                 # encoding: [0x00,0x00,0x44,0xbe]
  bltid r4, 0

# bleid r4, 0 — rD=0b10011=19 (LE + delay)
# Word: 0xBE640000  LE: 00 00 64 BE
# CHECK: bleid r4, 0                 # encoding: [0x00,0x00,0x64,0xbe]
  bleid r4, 0

# bgtid r4, 0 — rD=0b10100=20 (GT + delay)
# Word: 0xBE840000  LE: 00 00 84 BE
# CHECK: bgtid r4, 0                 # encoding: [0x00,0x00,0x84,0xbe]
  bgtid r4, 0

# bgeid r4, 0 — rD=0b10101=21 (GE + delay)
# Word: 0xBEA40000  LE: 00 00 A4 BE
# CHECK: bgeid r4, 0                 # encoding: [0x00,0x00,0xa4,0xbe]
  bgeid r4, 0

#------------------------------------------------------------------------------
# Additional Type B memory and arithmetic instructions
#------------------------------------------------------------------------------

# lbui r3, r5, 42  — load byte unsigned immediate; opcode=0x38
# Word: 0b111000_00011_00101_0000000000101010 = 0xE065002A  LE: 2A 00 65 E0
# CHECK: lbui r3, r5, 42             # encoding: [0x2a,0x00,0x65,0xe0]
  lbui r3, r5, 42

# lhui r3, r5, 42  — load halfword unsigned immediate; opcode=0x39
# Word: 0xE465002A  LE: 2A 00 65 E4
# CHECK: lhui r3, r5, 42             # encoding: [0x2a,0x00,0x65,0xe4]
  lhui r3, r5, 42

# sbi r3, r5, 42  — store byte immediate; opcode=0x3C
# Word: 0xF065002A  LE: 2A 00 65 F0
# CHECK: sbi r3, r5, 42              # encoding: [0x2a,0x00,0x65,0xf0]
  sbi r3, r5, 42

# shi r3, r5, 42  — store halfword immediate; opcode=0x3D
# Word: 0xF465002A  LE: 2A 00 65 F4
# CHECK: shi r3, r5, 42              # encoding: [0x2a,0x00,0x65,0xf4]
  shi r3, r5, 42

# xori r3, r5, 42  — XOR immediate; opcode=0x2A
# Word: 0xA865002A  LE: 2A 00 65 A8
# CHECK: xori r3, r5, 42             # encoding: [0x2a,0x00,0x65,0xa8]
  xori r3, r5, 42

# rsubk r3, r5, r7  — reverse subtract keep carry; opcode=0x05, func=0
# Word: 0x14653800  LE: 00 38 65 14
# CHECK: rsubk r3, r5, r7            # encoding: [0x00,0x38,0x65,0x14]
  rsubk r3, r5, r7

# rsubik r3, r5, 42  — reverse subtract immediate keep carry; opcode=0x0D
# Word: 0x3465002A  LE: 2A 00 65 34
# CHECK: rsubik r3, r5, 42           # encoding: [0x2a,0x00,0x65,0x34]
  rsubik r3, r5, 42
