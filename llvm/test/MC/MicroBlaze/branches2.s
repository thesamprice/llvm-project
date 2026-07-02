# RUN: llvm-mc -triple=microblazeel -show-encoding < %s | FileCheck %s
# REQUIRES: microblaze-registered-target

# MicroBlaze extended branch instruction encoding tests (UG984 §5).
# Bytes shown little-endian (low address first).
#
# Opcode 0x26 (100110) — unconditional reg-target branches
#   rA modifier = {delay[4], absolute[3], link[2], 0, 0}
# Opcode 0x27 (100111) — conditional reg-target branches
#   rD modifier = {delay[4], 0, 0, 0, cond[0]} or rD=D_COND
# Opcode 0x2E (101110) — Type B unconditional (BRI/BRID/BRAID already in instrinfo-branch)
# Opcode 0x2F (101111) — Type B conditional (BEQID..BGEID in instrinfo-branch)

#------------------------------------------------------------------------------
# Opcode 0x26 — unconditional register-target branches
# Type A  opcode[31:26] rD[25:21] rA[20:16] rB[15:11] func[10:0]
#
# br r5   — opcode=0x26, rD=0, rA=0, rB=5, func=0
# Word: 0b100110_00000_00000_00101_00000000000 = 0x98002800
# LE: 00 28 00 98
#------------------------------------------------------------------------------
# CHECK: br r5                        # encoding: [0x00,0x28,0x00,0x98]
  br r5

# brd r5  — rA=0b10000=16
# rA bits[20:16]=10000; byte2 = bits[23:16]: bit23=rD[2]=0..rD[0]=0 | bit20=rA[4]=1..rA[0]=0
# byte2 = 0b00010000 = 0x10
# Word 0x98102800 → LE: 00 28 10 98
# CHECK: brd r5                       # encoding: [0x00,0x28,0x10,0x98]
  brd r5

# bra r5  — rA=0b01000=8
# byte2: bits[20:16]=01000 → 0b00001000 = 0x08
# LE: 00 28 08 98
# CHECK: bra r5                       # encoding: [0x00,0x28,0x08,0x98]
  bra r5

# brad r5 — rA=0b11000=24
# byte2: bits[20:16]=11000 → 0b00011000 = 0x18
# LE: 00 28 18 98
# CHECK: brad r5                      # encoding: [0x00,0x28,0x18,0x98]
  brad r5

# brld r15, r5 — rD=r15=15=0b01111, rA=0b10100=20
# byte3: opcode=100110, rD[4:3]=01 → bits[31:24]=0b10011001 = 0x99... wait
# opcode=0x26=100110 → bits[31:26]=100110
# rD=15=0b01111 → bits[25:21]=01111
# bits[31:24]: 1,0,0,1,1,0,0,1 = 0x99
# bits[23:16]: rD[2:0]=111, rA[4:0]=10100 → 1,1,1,1,0,1,0,0 = 0xF4? wait
# Actually: bit23=rD[2]=1, bit22=rD[1]=1, bit21=rD[0]=1, bit20=rA[4]=1, bit19=rA[3]=0, bit18=rA[2]=1, bit17=rA[1]=0, bit16=rA[0]=0
# byte2 = 0b11110100 = 0xF4
# rB=r5: bits[15:11]=00101; byte1 = 0b00101000 = 0x28; byte0=0x00
# LE: 00 28 F4 99
# CHECK: brld r15, r5                 # encoding: [0x00,0x28,0xf4,0x99]
  brld r15, r5

# brald r15, r5 — rD=15=0b01111, rA=0b11100=28
# byte3: 100110_01 → 0b10011001 = 0x99 (same high byte)
# byte2: rD[2:0]=111, rA=11100 → 1,1,1,1,1,1,0,0 = 0xFC
# byte1: rB=5 → 0x28; byte0=0x00
# LE: 00 28 FC 99
# CHECK: brald r15, r5               # encoding: [0x00,0x28,0xfc,0x99]
  brald r15, r5

# brk r3, r5 — rD=3, rA=0b01100=12
# byte3: 100110_00 = 0x98
# byte2: rD[2:0]=011, rA=01100 → 0,1,1,0,1,1,0,0 = 0x6C
# byte1: rB=5 → 0x28; byte0=0x00
# LE: 00 28 6C 98
# CHECK: brk r3, r5                  # encoding: [0x00,0x28,0x6c,0x98]
  brk r3, r5

#------------------------------------------------------------------------------
# Opcode 0x27 — conditional register-target branches
# Type A  rD[25:21]=D_COND  rA[20:16]=test_reg  rB[15:11]=target_reg
#
# beq r4, r5 — opcode=0x27=100111, rD=0b00000, rA=r4=4, rB=r5=5
# byte3: 1,0,0,1,1,1,0,0 = 0x9C
# byte2: 0,0,0,0,0,1,0,0 = 0x04 (rD[2:0]=000, rA=00100=r4)
# byte1: rB=r5=5=00101, func=000 → 0b00101000 = 0x28; byte0=0x00
# LE: 00 28 04 9C
#------------------------------------------------------------------------------
# CHECK: beq r4, r5                  # encoding: [0x00,0x28,0x04,0x9c]
  beq r4, r5

# bne r4, r5 — rD=0b00001
# byte3: 0x9C; byte2: rD[2:0]=000, rA=00100 → 0x04; same rA
# Wait rD=1=0b00001: bit25=0,bit24=0,bit23=rD[2]=0,bit22=rD[1]=0,bit21=rD[0]=1
# byte2: rD[2:0]=0,0,1; rA[4:0]=0,0,1,0,0
# byte2 = 0b00100100 = 0x24? Wait:
# bit23=rD[2]=0, bit22=rD[1]=0, bit21=rD[0]=1, bit20=rA[4]=0, bit19=rA[3]=0, bit18=rA[2]=1, bit17=rA[1]=0, bit16=rA[0]=0
# byte2 = 0b00100100 = 0x24
# LE: 00 28 24 9C
# CHECK: bne r4, r5                  # encoding: [0x00,0x28,0x24,0x9c]
  bne r4, r5

# blt r4, r5 — rD=0b00010
# byte2: rD[2:0]=0,1,0; rA=00100 → 0b01000100 = 0x44
# LE: 00 28 44 9C
# CHECK: blt r4, r5                  # encoding: [0x00,0x28,0x44,0x9c]
  blt r4, r5

# ble r4, r5 — rD=0b00011
# byte2: 0,1,1,0,0,1,0,0 = 0x64
# LE: 00 28 64 9C
# CHECK: ble r4, r5                  # encoding: [0x00,0x28,0x64,0x9c]
  ble r4, r5

# bgt r4, r5 — rD=0b00100
# byte2: 1,0,0,0,0,1,0,0 = 0x84
# LE: 00 28 84 9C
# CHECK: bgt r4, r5                  # encoding: [0x00,0x28,0x84,0x9c]
  bgt r4, r5

# bge r4, r5 — rD=0b00101
# byte2: 1,0,1,0,0,1,0,0 = 0xA4
# LE: 00 28 A4 9C
# CHECK: bge r4, r5                  # encoding: [0x00,0x28,0xa4,0x9c]
  bge r4, r5

# Delay-slot forms — beqd/bned use rD bit4=1 (D=1)
# beqd r4, r5 — rD=0b10000=16=0x10
# byte3: 1,0,0,1,1,1,1,0 = 0x9E
# byte2: rD[2:0]=000; rA=00100 → 0b00000100 = 0x04
# LE: 00 28 04 9E
# CHECK: beqd r4, r5                 # encoding: [0x00,0x28,0x04,0x9e]
  beqd r4, r5

# bned r4, r5 — rD=0b10001
# byte3: 0x9E; byte2: rD[2:0]=001, rA=00100 → 0b00100100 = 0x24
# LE: 00 28 24 9E
# CHECK: bned r4, r5                 # encoding: [0x00,0x28,0x24,0x9e]
  bned r4, r5

#------------------------------------------------------------------------------
# Missing Type B unconditional — opcode 0x2E  (adds BRAI, BRLID, BRKI)
#------------------------------------------------------------------------------

# brai 256 — opcode=0x2E=101110, rD=0b01000=8, rA=0, imm=256=0x100
# byte3: 1,0,1,1,1,0,0,1 = 0xB9
# byte2: rD[2:0]=0,0,0; rA=0,0,0,0,0 → 0b00000000 = 0x00
# imm=0x0100; byte1=0x01, byte0=0x00
# LE: 00 01 00 B9
# CHECK: brai 256                    # encoding: [0x00,0x01,0x00,0xb9]
  brai 256

# brlid r15, 8 — rD=r15=15=0b01111, rA=0b10100=20, imm=8
# byte3: 1,0,1,1,1,0,0,1 = 0xB9 (opcode=0x2E=101110, rD[4:3]=01)
# byte2: rD[2:0]=111, rA=10100 → 1,1,1,1,0,1,0,0 = 0xF4
# imm=8: byte1=0x00, byte0=0x08
# LE: 08 00 F4 B9
# CHECK: brlid r15, 8                # encoding: [0x08,0x00,0xf4,0xb9]
  brlid r15, 8

# brki r3, 8 — rD=r3=3=0b00011, rA=0b01100=12, imm=8
# byte3: opcode=0x2E=101110, rD[4:3]=00 → 1,0,1,1,1,0,0,0 = 0xB8
# byte2: rD[2:0]=011, rA=01100 → 0,1,1,0,1,1,0,0 = 0x6C
# imm=8: 0x0008 → byte1=0x00, byte0=0x08
# LE: 08 00 6C B8
# CHECK: brki r3, 8                  # encoding: [0x08,0x00,0x6c,0xb8]
  brki r3, 8

#------------------------------------------------------------------------------
# Type B non-delay conditional branches — opcode 0x2F  (rD[4]=0, no delay)
#------------------------------------------------------------------------------

# beqi r4, 0 — opcode=0x2F=101111, rD=0b00000=0, rA=r4=4, imm=0
# byte3: 1,0,1,1,1,1,0,0 = 0xBC
# byte2: rD[2:0]=000, rA=00100 → 0b00000100 = 0x04
# LE: 00 00 04 BC
# CHECK: beqi r4, 0                  # encoding: [0x00,0x00,0x04,0xbc]
  beqi r4, 0

# bnei r4, 0 — rD=0b00001
# byte2: rD[2:0]=001, rA=00100 → 0b00100100 = 0x24
# LE: 00 00 24 BC
# CHECK: bnei r4, 0                  # encoding: [0x00,0x00,0x24,0xbc]
  bnei r4, 0

# blti r4, 0 — rD=0b00010
# byte2: 0b01000100 = 0x44
# LE: 00 00 44 BC
# CHECK: blti r4, 0                  # encoding: [0x00,0x00,0x44,0xbc]
  blti r4, 0

# blei r4, 0 — rD=0b00011
# byte2: 0b01100100 = 0x64
# LE: 00 00 64 BC
# CHECK: blei r4, 0                  # encoding: [0x00,0x00,0x64,0xbc]
  blei r4, 0

# bgti r4, 0 — rD=0b00100
# byte2: 0b10000100 = 0x84
# LE: 00 00 84 BC
# CHECK: bgti r4, 0                  # encoding: [0x00,0x00,0x84,0xbc]
  bgti r4, 0

# bgei r4, 0 — rD=0b00101
# byte2: 0b10100100 = 0xA4
# LE: 00 00 A4 BC
# CHECK: bgei r4, 0                  # encoding: [0x00,0x00,0xa4,0xbc]
  bgei r4, 0

#------------------------------------------------------------------------------
# Opcode 0x2E — remaining Type B unconditional forms
#------------------------------------------------------------------------------

# bri 8 — no link, no absolute, no delay; rD=0b00000=0, rA=0
# Word: 0xB8000008  LE: 08 00 00 B8
# CHECK: bri 8                        # encoding: [0x08,0x00,0x00,0xb8]
  bri 8

# brid 8 — delay slot, no absolute, no link; rD=0b10000=16
# Word: 0xBA000008  LE: 08 00 00 BA
# CHECK: brid 8                       # encoding: [0x08,0x00,0x00,0xba]
  brid 8

# braid 8 — absolute + delay, no link; rD=0b11000=24
# Word: 0xBB000008  LE: 08 00 00 BB
# CHECK: braid 8                      # encoding: [0x08,0x00,0x00,0xbb]
  braid 8

# bralid r15, 8 — absolute + link + delay; rD=r15=15, rA=0b11100=28
# Word: 0xB9FC0008  LE: 08 00 FC B9
# CHECK: bralid r15, 8               # encoding: [0x08,0x00,0xfc,0xb9]
  bralid r15, 8

#------------------------------------------------------------------------------
# Opcode 0x27 — remaining conditional delay-slot forms (bltd/bled/bgtd/bged)
#------------------------------------------------------------------------------

# bltd r4, r5 — LT + delay; rD=0b10010=18
# byte3: 0x9E; byte2: rD[2:0]=010, rA=00100 → 0x44
# LE: 00 28 44 9E
# CHECK: bltd r4, r5                  # encoding: [0x00,0x28,0x44,0x9e]
  bltd r4, r5

# bled r4, r5 — LE + delay; rD=0b10011=19
# byte3: 0x9E; byte2: 0x64
# LE: 00 28 64 9E
# CHECK: bled r4, r5                  # encoding: [0x00,0x28,0x64,0x9e]
  bled r4, r5

# bgtd r4, r5 — GT + delay; rD=0b10100=20
# byte3: 0x9E; byte2: 0x84
# LE: 00 28 84 9E
# CHECK: bgtd r4, r5                  # encoding: [0x00,0x28,0x84,0x9e]
  bgtd r4, r5

# bged r4, r5 — GE + delay; rD=0b10101=21
# byte3: 0x9E; byte2: 0xA4
# LE: 00 28 A4 9E
# CHECK: bged r4, r5                  # encoding: [0x00,0x28,0xa4,0x9e]
  bged r4, r5
