# RUN: llvm-mc -triple=microblazeel --filetype=obj -o %t %s
# RUN: llvm-objdump -d --triple=microblazeel %t | FileCheck %s
# REQUIRES: microblaze-registered-target

# Round-trip encode→assemble→disassemble test.
# Exercises every major instruction group to catch disassembler regressions.

#--- ALU register ---
# CHECK: addk  r3, r4, r5
  addk  r3, r4, r5
# CHECK: rsubk r3, r4, r5
  rsubk r3, r4, r5
# CHECK: and   r3, r4, r5
  and   r3, r4, r5
# CHECK: or    r3, r4, r5
  or    r3, r4, r5
# CHECK: xor   r3, r4, r5
  xor   r3, r4, r5
# CHECK: andn  r3, r4, r5
  andn  r3, r4, r5
# CHECK: mul   r3, r4, r5
  mul   r3, r4, r5

#--- ALU immediate ---
# CHECK: addik r3, r4, 1
  addik r3, r4, 1
# CHECK: rsubik r3, r4, 2
  rsubik r3, r4, 2
# CHECK: ori   r3, r4, 42
  ori   r3, r4, 42
# CHECK: andi  r3, r4, 255
  andi  r3, r4, 255
# CHECK: muli  r3, r4, 10
  muli  r3, r4, 10
# CHECK: nop
  or r0, r0, r0

#--- Memory load/store ---
# CHECK: lwi   r3, r1, 0
  lwi   r3, r1, 0
# CHECK: lhui  r3, r1, 4
  lhui  r3, r1, 4
# CHECK: lbui  r3, r1, 8
  lbui  r3, r1, 8
# CHECK: swi   r3, r1, 0
  swi   r3, r1, 0
# CHECK: shi   r3, r1, 4
  shi   r3, r1, 4
# CHECK: sbi   r3, r1, 8
  sbi   r3, r1, 8
# CHECK: lw    r3, r4, r5
  lw    r3, r4, r5
# CHECK: sw    r3, r4, r5
  sw    r3, r4, r5

#--- Shift / sign-extend ---
# CHECK: sra   r3, r4
  sra   r3, r4
# CHECK: srl   r3, r4
  srl   r3, r4
# CHECK: sext8 r3, r4
  sext8 r3, r4
# CHECK: sext16 r3, r4
  sext16 r3, r4

#--- Branches ---
# CHECK: bri   0
  bri   0
# CHECK: brid  0
  brid  0
# CHECK: beqid r4, 0
  beqid r4, 0
# CHECK: bneid r4, 0
  bneid r4, 0
# CHECK: bltid r4, 0
  bltid r4, 0
# CHECK: bleid r4, 0
  bleid r4, 0
# CHECK: bgtid r4, 0
  bgtid r4, 0
# CHECK: bgeid r4, 0
  bgeid r4, 0

#--- Return / special ---
# CHECK: rtsd  r15, 8
  rtsd  r15, 8
# CHECK: bralid r15, 0
  bralid r15, 0

#--- Special-purpose registers ---
# MSR is SPR register 1; disassembler prints numeric SPR id.
# CHECK: mfs   r3, 1
  mfs   r3, rmsr
# CHECK: mts   1, r3
  mts   rmsr, r3

#--- Carry arithmetic ---
# CHECK: add   r3, r4, r5
  add   r3, r4, r5
# CHECK: rsub  r3, r4, r5
  rsub  r3, r4, r5
# CHECK: addc  r3, r4, r5
  addc  r3, r4, r5
# CHECK: rsubc r3, r4, r5
  rsubc r3, r4, r5
