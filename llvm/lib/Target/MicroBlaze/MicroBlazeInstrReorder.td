//===-- MicroBlazeInstrReorder.td - MicroBlaze Instruction Definitions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//   SRA    0x001 = 0b00000000001  — arithmetic right shift by 1; MSR.C ← bit[0]
//   SRC    0x021 = 0b00000100001  — shift right through carry (MSR.C in/out)
//   SRL    0x041 = 0b01000000001  — logical right shift by 1; MSR.C ← bit[0]
//   SWAPB  0x1E0 = 0b00111100000 — swap 4 bytes  (requires +reorder)
//   SWAPH  0x1E2 = 0b00111100010 — swap 2 halfwords (requires +reorder)
//===----------------------------------------------------------------------===//
// Byte/halfword reorder instructions (require +reorder, opcode 0x24)
// UG984 §5 Figs 137-138. Latency: 1 cycle.
//   SWAPB  func=0x1E0 (bits[8:5]=1111) — byte-reverse a 32-bit word
//   SWAPH  func=0x1E2 (bits[8:5]=1111, bit[1]=1) — halfword-swap a 32-bit word
//===----------------------------------------------------------------------===//

let Predicates = [HasReorderInstrPred] in {
  def SWAPB : MBA<0x24, 0x1E0, (outs GPR:$rD), (ins GPR:$rA),
                  "swapb\t$rD, $rA", []> {
    let rB = 0;
  }
  def SWAPH : MBA<0x24, 0x1E2, (outs GPR:$rD), (ins GPR:$rA),
                  "swaph\t$rD, $rA", []> {
    let rB = 0;
  }

  // bswap32(x) = swaph(swapb(x)):
  //   swapb: ABCD → BADC  (swap bytes within each halfword)
  //   swaph: BADC → DCBA  (swap the two halfwords)
  // Two instructions vs the shift/or expansion (8+ instructions).
  def : Pat<(bswap i32:$rA), (SWAPH (SWAPB GPR:$rA))>;
}

