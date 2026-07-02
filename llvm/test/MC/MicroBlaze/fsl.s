# RUN: llvm-mc -triple=microblazeel -show-encoding -mattr=+fsl < %s | FileCheck %s
# REQUIRES: microblaze-registered-target

# MicroBlaze FSL/AXI-Stream instruction encoding tests (UG984 §5).
# Two families:
#   Static  (opcode 0x1B, 0x6C______): port number in bits[7:4] as rfsl0..rfsl15
#   Dynamic (opcode 0x13, 0x4C______): port from GPR rB in bits[15:11]
#
# Flag positions in static encoding: P[15] N[14] C[13] T[12] A[11] E[10]
# Flag positions in dynamic encoding: P[10] N[9] C[8] T[7] A[6] E[5]
# P=0: get (read from FIFO into GPR); P=1: put (write GPR to FIFO)

# --- Static GET (opcode 0x1B = 0x6C______, P=0) ---

# get r3, rfsl0  — no flags; N=C=T=A=E=0, port=0
# Word = 0x6C<<24 | (3<<21) | (0<<16) | 0<<15 | port=0 → 0x6C600000  LE: [0x00,0x00,0x60,0x6c]
# CHECK: get r3, rfsl0                # encoding: [0x00,0x00,0x60,0x6c]
  get r3, rfsl0

# eget r3, rfsl1  — E=1; bits[10]=1, port=1 → adds 0x0400 and 0x0010
# CHECK: eget r3, rfsl1               # encoding: [0x10,0x04,0x60,0x6c]
  eget r3, rfsl1

# tnecaget r6, rfsl15 — all flags set (T=N=E=C=A=1), port=15
# bits[14:10] = N=1,C=1,T=1,A=1,E=1 → 0x7C00; port=15 → bits[7:4]=0xF0
# CHECK: tnecaget r6, rfsl15          # encoding: [0xf0,0x7c,0xc0,0x6c]
  tnecaget r6, rfsl15

# --- Static PUT (opcode 0x1B = 0x6C______, P=1, bit[15]=1) ---

# put r5, rfsl4  — no flags; P=1, port=4
# CHECK: put r5, rfsl4                # encoding: [0x40,0x80,0x05,0x6c]
  put r5, rfsl4

# tnecaput r5, rfsl0 — all flags, port=0
# CHECK: tnecaput r5, rfsl0           # encoding: [0x00,0xfc,0x05,0x6c]
  tnecaput r5, rfsl0

# --- Dynamic GET (opcode 0x13 = 0x4C______, P=0) ---

# getd r3, r5  — no flags; rB=r5
# bits[15:11]=rB=5 → 0x0028; bits[10:5]=0; P=0
# CHECK: getd r3, r5                  # encoding: [0x00,0x28,0x60,0x4c]
  getd r3, r5

# tnecagetd r7, r8  — all flags set (N[9]=C[8]=T[7]=A[6]=E[5]=1), rB=r8
# bits[15:11]=8 → 0x0040 shifted: (8<<11)=0x4000; P=0[10]=0; N[9]=C[8]=T[7]=A[6]=E[5]=1
# = 0x43E0 in low 16; rD=7 → bits[25:21]=7
# CHECK: tnecagetd r7, r8             # encoding: [0xe0,0x43,0xe0,0x4c]
  tnecagetd r7, r8

# --- Dynamic PUT (opcode 0x13 = 0x4C______, P=1, bit[10]=1) ---

# putd r5, r6  — no flags; rB=r6, P=1
# CHECK: putd r5, r6                  # encoding: [0x00,0x34,0x05,0x4c]
  putd r5, r6

# tnecaputd r7, r8  — all flags, rB=r8, P=1
# CHECK: tnecaputd r7, r8             # encoding: [0xe0,0x47,0x07,0x4c]
  tnecaputd r7, r8
