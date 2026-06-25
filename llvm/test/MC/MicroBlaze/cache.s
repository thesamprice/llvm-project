# RUN: llvm-mc -triple=microblazeel -show-encoding < %s | FileCheck %s
# REQUIRES: microblaze-registered-target

# MicroBlaze cache control instruction encoding tests (UG984 §5 Figs 141-142).
# Base ISA — no feature flag required.
#
# Type A format: [31:26]=0x24(100100) [25:21]=rD(=0) [20:16]=rA [15:11]=rB [10:0]=func
#
# WDC (Fig 141) func field: base=0x064 (bits[6,5,2]=1) + control bits:
#   E(bit[10]) EA(bit[7]) F(bit[4]) T(bit[1])
#   wdc:           func=0x064  (no control bits)
#   wdc.flush:     func=0x074  (F=1)
#   wdc.clear:     func=0x066  (T=1)
#   wdc.clear.ea:  func=0x0E6  (T=1, EA=1→bit[7])
#   wdc.ext.flush: func=0x476  (E=1, F=1, T=1)
#   wdc.ext.clear: func=0x466  (E=1, T=1)
# WIC (Fig 142) func=0x068 (bits[6,5,3]=1). Latency: 2 cycles.

# For all tests: rA=r5=5, rB=r7=7, rD=0 (fixed).
# Base word (no func): 0x24<<26 | 0 | 5<<16 | 7<<11 = 0x90053800
# +func: byte 0 = func[7:0], byte 1 high nibble = func[10:8] merged with rB[4:0].

#------------------------------------------------------------------------------
# wdc rA, rB — invalidate data cache line at rA+rB  (func=0x064)
# Word = 0x90053800 | 0x064 = 0x90053864
# LE: 64 38 05 90
#------------------------------------------------------------------------------
# CHECK: wdc r5, r7                    # encoding: [0x64,0x38,0x05,0x90]
  wdc r5, r7

# wdc.flush rA, rB — flush+invalidate cache line  (func=0x074, F=1)
# LE: 74 38 05 90
# CHECK: wdc.flush r5, r7              # encoding: [0x74,0x38,0x05,0x90]
  wdc.flush r5, r7

# wdc.clear rA, rB — invalidate matching address only  (func=0x066, T=1)
# LE: 66 38 05 90
# CHECK: wdc.clear r5, r7              # encoding: [0x66,0x38,0x05,0x90]
  wdc.clear r5, r7

# wdc.clear.ea rA, rB — clear with extended address  (func=0x0E6, T=1, EA=1→bit[7])
# LE: E6 38 05 90
# CHECK: wdc.clear.ea r5, r7           # encoding: [0xe6,0x38,0x05,0x90]
  wdc.clear.ea r5, r7

# wdc.ext.flush rA, rB — ext cache flush+invalidate  (func=0x476, E=1,F=1,T=1)
# func[10:8]=100; byte1 = rB[4:0]=00111, 100 → 0x3C; byte0=0x76
# LE: 76 3C 05 90
# CHECK: wdc.ext.flush r5, r7          # encoding: [0x76,0x3c,0x05,0x90]
  wdc.ext.flush r5, r7

# wdc.ext.clear rA, rB — ext cache invalidate  (func=0x466, E=1,T=1)
# byte1=0x3C; byte0=0x66
# LE: 66 3C 05 90
# CHECK: wdc.ext.clear r5, r7          # encoding: [0x66,0x3c,0x05,0x90]
  wdc.ext.clear r5, r7

#------------------------------------------------------------------------------
# wic rA, rB — invalidate instruction cache line at rA  (func=0x068)
# Word = 0x90053800 | 0x068 = 0x90053868
# LE: 68 38 05 90
#------------------------------------------------------------------------------
# CHECK: wic r5, r7                    # encoding: [0x68,0x38,0x05,0x90]
  wic r5, r7
