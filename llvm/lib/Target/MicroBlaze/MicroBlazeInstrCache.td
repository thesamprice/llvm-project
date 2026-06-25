//===-- MicroBlazeInstrCache.td - MicroBlaze Instruction Definitions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Cache control instructions (Type A, opcode 0x24, rD=0)
// UG984 §5 Figs 141-142. Base ISA. hasSideEffects=1 (write to cache tags).
//
// WDC variants (Fig 141): func field = fixed{6,5,2} | E{10} | EA{7} | F{4} | T{1}
//   func base = 0x064 (bits 6,5,2 always set)
//   wdc:           func=0x064  (no control bits)
//   wdc.flush:     func=0x074  (F=1 → bit[4])
//   wdc.clear:     func=0x066  (T=1 → bit[1])
//   wdc.clear.ea:  func=0x0E6  (T=1, EA=1 → bits[7,1])
//   wdc.ext.flush: func=0x476  (E=1,F=1,T=1 → bits[10,4,1])
//   wdc.ext.clear: func=0x466  (E=1,T=1 → bits[10,1])
//
// WIC (Fig 142): func=0x068  (bits[6,5,3] fixed; rB unused but present)
//   Latency: 2 cycles.
//===----------------------------------------------------------------------===//

let hasSideEffects = 1 in {
  def WDC          : MBA<0x24, 0x064, (outs), (ins GPR:$rA, GPR:$rB),
                         "wdc\t$rA, $rB", []> { let rD = 0; }
  def WDC_FLUSH    : MBA<0x24, 0x074, (outs), (ins GPR:$rA, GPR:$rB),
                         "wdc.flush\t$rA, $rB", []> { let rD = 0; }
  def WDC_CLEAR    : MBA<0x24, 0x066, (outs), (ins GPR:$rA, GPR:$rB),
                         "wdc.clear\t$rA, $rB", []> { let rD = 0; }
  def WDC_CLEAR_EA : MBA<0x24, 0x0E6, (outs), (ins GPR:$rA, GPR:$rB),
                         "wdc.clear.ea\t$rA, $rB", []> { let rD = 0; }
  def WDC_EXT_FLUSH: MBA<0x24, 0x476, (outs), (ins GPR:$rA, GPR:$rB),
                         "wdc.ext.flush\t$rA, $rB", []> { let rD = 0; }
  def WDC_EXT_CLEAR: MBA<0x24, 0x466, (outs), (ins GPR:$rA, GPR:$rB),
                         "wdc.ext.clear\t$rA, $rB", []> { let rD = 0; }
  def WIC          : MBA<0x24, 0x068, (outs), (ins GPR:$rA, GPR:$rB),
                         "wic\t$rA, $rB", []> { let rD = 0; }
}

