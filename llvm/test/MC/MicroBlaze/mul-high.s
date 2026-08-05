# RUN: llvm-mc -triple=microblazeel -show-encoding -mattr=+multiply-high < %s | FileCheck %s
# REQUIRES: microblaze-registered-target

# MicroBlaze multiply-high instruction encoding tests (UG984 §5).
# All are Type A: opcode=0x10 (010000), func field selects variant.
# Type A: opcode[31:26] rD[25:21] rA[20:16] rB[15:11] func[10:0]
#
# Using rD=r3, rA=r5, rB=r7 throughout.
# Word = 0x40000000 | (3<<21) | (5<<16) | (7<<11) | func

# mulh r3, r5, r7  — signed multiply, high 32 bits; func=0x001
# Word = 0x40653801  LE: [0x01,0x38,0x65,0x40]
# CHECK: mulh r3, r5, r7              # encoding: [0x01,0x38,0x65,0x40]
  mulh r3, r5, r7

# mulhsu r3, r5, r7  — signed×unsigned multiply, high 32 bits; func=0x002
# Word = 0x40653802  LE: [0x02,0x38,0x65,0x40]
# CHECK: mulhsu r3, r5, r7            # encoding: [0x02,0x38,0x65,0x40]
  mulhsu r3, r5, r7

# mulhu r3, r5, r7  — unsigned multiply, high 32 bits; func=0x003
# Word = 0x40653803  LE: [0x03,0x38,0x65,0x40]
# CHECK: mulhu r3, r5, r7             # encoding: [0x03,0x38,0x65,0x40]
  mulhu r3, r5, r7
