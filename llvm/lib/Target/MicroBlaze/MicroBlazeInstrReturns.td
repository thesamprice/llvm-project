//===-- MicroBlazeInstrReturns.td - MicroBlaze Instruction Definitions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// Special return instructions (Type B, opcode 0x2D) and memory barrier
// UG984 §5: rD field encodes the return type.  Latency: 1 cycle.
//
// rD  Return type
// 0b10000  rtsd — subroutine  (already defined in instrinfo-branch)
// 0b10001  rtid — interrupt
// 0b10010  rtbd — break
// 0b10100  rted — exception
//===----------------------------------------------------------------------===//

// RTID/RTBD/RTED restore processor state from special registers (MSR, BTR,
// EAR, ESR…) that are not modeled as physical registers.  hasSideEffects=1
// captures those implicit state changes.
let hasSideEffects = 1 in {

// RTID Ra, imm — return from interrupt; resumes at Ra + sext(imm).
def RTID : MBB<0x2D, (outs), (ins GPR:$rA, simm16:$imm),
               "rtid\t$rA, $imm", []> {
  let rD = 0b10001;
  let isReturn = 1; let isTerminator = 1; let isBarrier = 1; let hasDelaySlot = 1;
}

// RTBD Ra, imm — return from break.
def RTBD : MBB<0x2D, (outs), (ins GPR:$rA, simm16:$imm),
               "rtbd\t$rA, $imm", []> {
  let rD = 0b10010;
  let isReturn = 1; let isTerminator = 1; let isBarrier = 1; let hasDelaySlot = 1;
}

// RTED Ra, imm — return from exception.
def RTED : MBB<0x2D, (outs), (ins GPR:$rA, simm16:$imm),
               "rted\t$rA, $imm", []> {
  let rD = 0b10100;
  let isReturn = 1; let isTerminator = 1; let isBarrier = 1; let hasDelaySlot = 1;
}

} // let hasSideEffects = 1

// Interrupt-handler return (cc73): rtid r14, 0 — resume at the interrupt
// return address held in R14 (UG984 Table 93).
def : Pat<(MBIntrRet), (RTID R14, 0)>;

// MBAR sel — memory barrier (opcode 0x2E, UG984 §2, §5).
// Encoding: [31:26]=0x2E  [25:21]=sel  [20:16]=0b00010  [15:0]=0x0004
//   sel=0 — full barrier + BTC flush (use sparingly; disrupts branch prediction)
//   sel=1 — data-side barrier (imm[0]=1): drains pending data-memory ops
//   sel=2 — instruction-side barrier (imm[1]=1): flushes I-cache pipeline
//   sel=3 — data + instruction barrier, no BTC flush
//   sel=8 — BTC flush only
//
// hasSideEffects=1 models the hardware memory-barrier side effects and also
// sets MCID::UnmodeledSideEffects so the delay-slot filler excludes MBAR.

// MBMBAR format: selector→rD[25:21], rA[20:16]=0b00010 fixed, imm=0x0004 fixed.
// Per QEMU insns.decode line 186:
//   mbar  101110 imm:5 00010 0000 0000 0000 0100
class MBMBAR<dag outs, dag ins, string asmstr, list<dag> pattern>
    : MBInst<outs, ins, asmstr, pattern, [WriteMBAR]> {
  bits<5> sel;
  let Inst{31-26} = 0x2E;
  let Inst{25-21} = sel;
  let Inst{20-16} = 0b00010;
  let Inst{15-0}  = 0x0004;
}

def MBAR : MBMBAR<(outs), (ins uimm5:$sel), "mbar\t$sel", []> {
  let hasSideEffects = 1;
}
