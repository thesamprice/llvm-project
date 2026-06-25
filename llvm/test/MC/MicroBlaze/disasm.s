# RUN: llvm-mc -triple=microblazeel --filetype=obj -o %t %s
# RUN: llvm-objdump -d --triple=microblazeel %t | FileCheck %s
# REQUIRES: microblaze-registered-target

# Round-trip disassembly test: assemble then disassemble.

# CHECK: addk  r3, r4, r5
  addk r3, r4, r5
# CHECK: nop
  or r0, r0, r0
# CHECK: and   r3, r4, r5
  and r3, r4, r5
# CHECK: xor   r3, r4, r5
  xor r3, r4, r5
# CHECK: andn  r3, r4, r5
  andn r3, r4, r5
# CHECK: mul   r3, r4, r5
  mul r3, r4, r5
# CHECK: addik r3, r4, 1
  addik r3, r4, 1
# CHECK: ori   r3, r4, 42
  ori r3, r4, 42
# CHECK: andi  r3, r4, 255
  andi r3, r4, 255
# CHECK: muli  r3, r4, 10
  muli r3, r4, 10
# CHECK: rtsd  r15, 8
  rtsd r15, 8
# CHECK: lwi   r3, r1, 0
  lwi r3, r1, 0
# CHECK: swi   r3, r1, 4
  swi r3, r1, 4
# CHECK: beqid r4, 0
  beqid r4, 0
# CHECK: bneid r4, 0
  bneid r4, 0
