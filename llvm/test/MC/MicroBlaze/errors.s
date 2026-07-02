# RUN: not llvm-mc -triple=microblazeel --filetype=obj %s 2>&1 | FileCheck %s
# REQUIRES: microblaze-registered-target

# AsmParser error-recovery: unknown mnemonics, wrong operand kinds/counts,
# and feature-gated instructions without the required feature flag.

#--- Unknown mnemonic ---
# CHECK: [[@LINE+1]]:{{[0-9]+}}: error: unrecognized instruction mnemonic
  noinstr r3, r4, r5

# CHECK: [[@LINE+1]]:{{[0-9]+}}: error: unrecognized instruction mnemonic
  addk.w r3, r4, r5

#--- Wrong operand type (imm where reg expected) ---
# addk is a Type A (reg+reg+reg) instruction; an immediate as rB is invalid.
# CHECK: [[@LINE+1]]:{{[0-9]+}}: error: invalid operand for instruction
  addk r3, r4, 5

# swi is mem-immediate (reg+reg+imm); passing a third register is invalid.
# CHECK: [[@LINE+1]]:{{[0-9]+}}: error: invalid operand for instruction
  swi r3, r1, r4

#--- Missing operand ---
# addk expects three registers; only two provided.
# CHECK: [[@LINE+1]]:{{[0-9]+}}: error: invalid operand for instruction
  addk r3, r4

#--- Feature-gated instructions without required feature ---
# bsrl requires +barrel-shift (not enabled by default).
# CHECK: [[@LINE+1]]:{{[0-9]+}}: error: instruction requires a target feature not enabled
  bsrl r3, r4, r5

# idiv requires +divide (not enabled by default).
# CHECK: [[@LINE+1]]:{{[0-9]+}}: error: instruction requires a target feature not enabled
  idiv r3, r4, r5

# flt requires +hard-float (not enabled by default).
# CHECK: [[@LINE+1]]:{{[0-9]+}}: error: instruction requires a target feature not enabled
  flt r3, r4
