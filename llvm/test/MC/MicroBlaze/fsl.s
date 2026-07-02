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
# CHECK: get r3, rfsl0                # encoding: [0x00,0x00,0x60,0x6c]
  get r3, rfsl0

# eget r3, rfsl1  — E=1; bits[10]=1, port=1
# CHECK: eget r3, rfsl1               # encoding: [0x10,0x04,0x60,0x6c]
  eget r3, rfsl1

# tnecaget r6, rfsl15 — all flags set (T=N=E=C=A=1), port=15
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
# CHECK: getd r3, r5                  # encoding: [0x00,0x28,0x60,0x4c]
  getd r3, r5

# tnecagetd r7, r8  — all flags set, rB=r8
# CHECK: tnecagetd r7, r8             # encoding: [0xe0,0x43,0xe0,0x4c]
  tnecagetd r7, r8

# --- Dynamic PUT (opcode 0x13 = 0x4C______, P=1, bit[10]=1) ---

# putd r5, r7  — no flags; rA=r5, rB=r7
# CHECK: putd r5, r7                  # encoding: [0x00,0x3c,0x05,0x4c]
  putd r5, r7
