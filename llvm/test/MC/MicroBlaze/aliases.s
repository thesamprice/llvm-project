# RUN: llvm-mc -triple=microblazeel -show-encoding < %s | FileCheck %s
# REQUIRES: microblaze-registered-target

# MicroBlaze assembler alias tests (UG984 §5 / binutils microblaze-opc.h).
# All aliases are parse-only (Emit=0); the printer outputs the canonical form.

# la Rd, Ra, imm  →  addik Rd, Ra, imm
# CHECK: addik r3, r5, 42              # encoding: [0x2a,0x00,0x65,0x30]
  la r3, r5, 42

# neg Rd, Ra  →  rsubk Rd, Ra, r0  (0 - Ra)
# CHECK: rsubk r3, r5, r0             # encoding: [0x00,0x00,0x65,0x14]
  neg r3, r5

# not Rd, Ra  →  xori Rd, Ra, -1
# CHECK: xori r3, r5, -1              # encoding: [0xff,0xff,0x65,0xa8]
  not r3, r5

# sub Rd, Ra, Rb  →  rsubk Rd, Rb, Ra  (operands swapped: Rd = Rb - Ra)
# CHECK: rsubk r3, r7, r5             # encoding: [0x00,0x28,0x67,0x14]
  sub r3, r5, r7

# rtb Ra  →  rtbd Ra, 4  (return from trap break)
# CHECK: rtbd r15, 4                  # encoding: [0x04,0x00,0x4f,0xb6]
  rtb r15

# tuqula Rd  →  addik Rd, r0, 42
# CHECK: addik r3, r0, 42             # encoding: [0x2a,0x00,0x60,0x30]
  tuqula r3

# Power management aliases
# hibernate  →  mbar 8
# CHECK: mbar 8                       # encoding: [0x08,0x00,0x40,0xb8]
  hibernate

# sleep  →  mbar 16
# CHECK: mbar 16                      # encoding: [0x10,0x00,0x40,0xb8]
  sleep

# suspend  →  mbar 24
# CHECK: mbar 24                      # encoding: [0x18,0x00,0x40,0xb8]
  suspend
