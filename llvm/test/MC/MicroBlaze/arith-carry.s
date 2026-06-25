# RUN: llvm-mc -triple=microblazeel -show-encoding < %s | FileCheck %s
# REQUIRES: microblaze-registered-target

# MicroBlaze carry-arithmetic instruction encoding tests (UG984 §5 Figs 59-60).
# All instructions are 32-bit, big-endian bit-numbered; bytes shown LE (low addr first).
#
# Type A  opcode[31:26] rD[25:21] rA[20:16] rB[15:11] func[10:0]
# Type B  opcode[31:26] rD[25:21] rA[20:16] imm16[15:0]
#
# opcode  K  C  add/rsub
#  0x00   0  0   add   |   0x01  0  0  rsub
#  0x02   0  1   addc  |   0x03  0  1  rsubc
#  0x04   1  0   addk  |   0x05  1  0  rsubk   (instrinfo-alu)
#  0x06   1  1   addkc |   0x07  1  1  rsubkc
# Immediate:  0x08..0x0F  (same K/C scheme + bit[26]=0/1 for add/rsub)

#------------------------------------------------------------------------------
# Type A — carry-add variants
#------------------------------------------------------------------------------

# add r3, r5, r7  — opcode=0x00, rD=3, rA=5, rB=7, func=0
# Word: 0b000000_00011_00101_00111_00000000000 = 0x00653800
# LE bytes: 00 38 65 00
# CHECK: add r3, r5, r7             # encoding: [0x00,0x38,0x65,0x00]
  add r3, r5, r7

# addc r3, r5, r7  — opcode=0x02
# Word: 0b000010_00011_00101_00111_00000000000 = 0x08653800
# LE bytes: 00 38 65 08
# CHECK: addc r3, r5, r7            # encoding: [0x00,0x38,0x65,0x08]
  addc r3, r5, r7

# addkc r3, r5, r7  — opcode=0x06 (K=1 C=1)
# Word: 0b000110_00011_00101_00111_00000000000 = 0x18653800
# LE bytes: 00 38 65 18
# CHECK: addkc r3, r5, r7           # encoding: [0x00,0x38,0x65,0x18]
  addkc r3, r5, r7

#------------------------------------------------------------------------------
# Type A — carry-subtract variants
#------------------------------------------------------------------------------

# rsub r3, r5, r7  — opcode=0x01
# Word: 0b000001_00011_00101_00111_00000000000 = 0x04653800
# LE bytes: 00 38 65 04
# CHECK: rsub r3, r5, r7            # encoding: [0x00,0x38,0x65,0x04]
  rsub r3, r5, r7

# rsubc r3, r5, r7  — opcode=0x03
# Word: 0b000011_00011_00101_00111_00000000000 = 0x0C653800
# LE bytes: 00 38 65 0C
# CHECK: rsubc r3, r5, r7           # encoding: [0x00,0x38,0x65,0x0c]
  rsubc r3, r5, r7

# rsubkc r3, r5, r7  — opcode=0x07 (K=1 C=1 rsub)
# Word: 0b000111_00011_00101_00111_00000000000 = 0x1C653800
# LE bytes: 00 38 65 1C
# CHECK: rsubkc r3, r5, r7          # encoding: [0x00,0x38,0x65,0x1c]
  rsubkc r3, r5, r7

#------------------------------------------------------------------------------
# Type B — carry-add immediate variants
#------------------------------------------------------------------------------

# addi r3, r5, 1  — opcode=0x08
# Word: 0b001000_00011_00101_0000000000000001 = 0x20650001
# LE bytes: 01 00 65 20
# CHECK: addi r3, r5, 1             # encoding: [0x01,0x00,0x65,0x20]
  addi r3, r5, 1

# addic r3, r5, 1  — opcode=0x0A
# Word: 0b001010_00011_00101_0000000000000001 = 0x28650001
# LE bytes: 01 00 65 28
# CHECK: addic r3, r5, 1            # encoding: [0x01,0x00,0x65,0x28]
  addic r3, r5, 1

# addikc r3, r5, 1  — opcode=0x0E
# Word: 0b001110_00011_00101_0000000000000001 = 0x38650001
# LE bytes: 01 00 65 38
# CHECK: addikc r3, r5, 1           # encoding: [0x01,0x00,0x65,0x38]
  addikc r3, r5, 1

#------------------------------------------------------------------------------
# Type B — carry-subtract immediate variants
#------------------------------------------------------------------------------

# rsubi r3, r5, 1  — opcode=0x09
# Word: 0b001001_00011_00101_0000000000000001 = 0x24650001
# LE bytes: 01 00 65 24
# CHECK: rsubi r3, r5, 1            # encoding: [0x01,0x00,0x65,0x24]
  rsubi r3, r5, 1

# rsubic r3, r5, 1  — opcode=0x0B
# Word: 0b001011_00011_00101_0000000000000001 = 0x2C650001
# LE bytes: 01 00 65 2C
# CHECK: rsubic r3, r5, 1           # encoding: [0x01,0x00,0x65,0x2c]
  rsubic r3, r5, 1

# rsubikc r3, r5, 1  — opcode=0x0F
# Word: 0b001111_00011_00101_0000000000000001 = 0x3C650001
# LE bytes: 01 00 65 3C
# CHECK: rsubikc r3, r5, 1          # encoding: [0x01,0x00,0x65,0x3c]
  rsubikc r3, r5, 1

#------------------------------------------------------------------------------
# ANDNI — logical AND NOT immediate (opcode=0x2B, UG984 §5 Fig 64). 1 cycle.
#------------------------------------------------------------------------------

# andni r3, r5, 255  — opcode=0x2B, rD=3, rA=5, imm=0xFF
# Word: 0b101011_00011_00101_0000000011111111 = 0xAC6500FF
# LE bytes: FF 00 65 AC
# CHECK: andni r3, r5, 255          # encoding: [0xff,0x00,0x65,0xac]
  andni r3, r5, 255
