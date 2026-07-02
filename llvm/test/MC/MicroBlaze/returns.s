# RUN: llvm-mc -triple=microblazeel -show-encoding < %s | FileCheck %s
# REQUIRES: microblaze-registered-target

# MicroBlaze special return and memory barrier encoding tests (UG984 §5).
# All use Type B format: opcode[31:26] rD[25:21] rA[20:16] imm16[15:0]
#
# opcode 0x2D (101101) for RTID/RTBD/RTED; rD encodes return type:
#   0b10000 = rtsd (subroutine)  — instrinfo-branch
#   0b10001 = rtid (interrupt)
#   0b10010 = rtbd (break)
#   0b10100 = rted (exception)
# opcode 0x2E (101110) for MBAR: sel→rD[25:21], rA=0b00010 (fixed), imm=0x0004 (fixed)

# rtid r14, 0 — opcode=0x2D, rD=0b10001=17, rA=r14=14, imm=0
# byte3: 1,0,1,1,0,1,1,0 = 0xB6 (opcode=101101, rD[4:3]=10)
# byte2: rD[2:0]=001, rA[4:0]=01110 = 0b00101110 = 0x2E
# imm=0 → byte1=0x00, byte0=0x00
# LE: 00 00 2E B6
# CHECK: rtid r14, 0                 # encoding: [0x00,0x00,0x2e,0xb6]
  rtid r14, 0

# rtbd r16, 0 — rD=0b10010=18, rA=r16=16
# byte3: opcode=101101, rD[4:3]=10 → 0xB6 (same)
# byte2: rD[2:0]=010, rA[4:0]=10000 → 0,1,0,1,0,0,0,0 = 0x50
# LE: 00 00 50 B6
# CHECK: rtbd r16, 0                 # encoding: [0x00,0x00,0x50,0xb6]
  rtbd r16, 0

# rted r17, 0 — rD=0b10100=20, rA=r17=17
# byte3: 0xB6
# byte2: rD[2:0]=100, rA[4:0]=10001 → 1,0,0,1,0,0,0,1 = 0x91
# LE: 00 00 91 B6
# CHECK: rted r17, 0                 # encoding: [0x00,0x00,0x91,0xb6]
  rted r17, 0

# mbar 0 — opcode=0x2E=101110, rD=0b00000 (sel=0), rA=0b00010 (fixed), imm=0x0004 (fixed)
# byte3: 1,0,1,1,1,0,0,0 = 0xB8 (opcode=101110, rD[4:3]=00)
# byte2: rD[2:0]=000, rA=00010 → 0,0,0,0,0,0,1,0 = 0x02
# LE: 04 00 02 B8
# CHECK: mbar 0                      # encoding: [0x04,0x00,0x02,0xb8]
  mbar 0
