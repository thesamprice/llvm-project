//===-- MicroBlazeInstrBranch.td - MicroBlaze Instruction Definitions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// Unconditional branch instructions (opcode=0x2E)
//===----------------------------------------------------------------------===//
//
// rD[25:21]: link register (only for *LID variants; 0 otherwise).
// rA[20:16]: branch modifier flags (UG984 Ch.5):
//   rA[4] (bit 20) = D  — delay slot
//   rA[3] (bit 19) = A  — absolute address
//   rA[2] (bit 18) = L  — link (store PC+4 in rD)

// BRI imm — relative, no delay slot
def BRI : MBB<0x2E, (outs), (ins brtarget:$imm), "bri\t$imm", []> {
  let rD = 0b00000;
  let rA = 0b00000;
  let isBranch    = 1;
  let isTerminator = 1;
  let isBarrier   = 1;
}

// BRID imm — relative, with delay slot
def BRID : MBB<0x2E, (outs), (ins brtarget:$imm), "brid\t$imm", []> {
  let rD = 0b00000;
  let rA = 0b10000; // D at bit 20
  let isBranch    = 1;
  let isTerminator = 1;
  let isBarrier   = 1;
  let hasDelaySlot = 1;
}

// BRAID imm — absolute, with delay slot
def BRAID : MBB<0x2E, (outs), (ins calltarget:$imm), "braid\t$imm", []> {
  let rD = 0b00000;
  let rA = 0b11000; // D at bit 20, A at bit 19
  let isBranch    = 1;
  let isTerminator = 1;
  let isBarrier   = 1;
  let hasDelaySlot = 1;
}

// BRALID Rd, imm — branch absolute with link and delay slot (direct call).
// rD field = link register (r15); rA field = 0b11100 encodes abs|link|delay.
// Mnemonic is bralid (Type B, imm target); brald is the Type A (reg) variant.
// UG984 Ch.5 (opcode=0x2E, rA modifier=11100).
def BRALID : MBB<0x2E, (outs), (ins GPR:$rD, calltarget:$imm),
                 "bralid\t$rD, $imm", []> {
  let rA = 0b11100; // abs | link | delay
  let isCall      = 1;
  let hasDelaySlot = 1;
  let Defs = [R15];
}

//===----------------------------------------------------------------------===//
// Conditional branch instructions (opcode=0x2F, all with delay slot)
//===----------------------------------------------------------------------===//
// rD[4:0] encodes delay-slot bit (bit4) and 4-bit condition (bits[3:0]).
// rA = register tested against zero; imm = PC-relative offset.
// Condition map (bits[24:21] in instruction word), UG984 Ch.5:
//   EQ=0000  NE=0001  LT=0010  LE=0011  GT=0100  GE=0101
// With delay bit (bit25=1), rD field = 0b1_COND where COND is the 4-bit code.

class CBranchID<bits<5> cc, string mnem>
    : MBB<0x2F, (outs), (ins GPR:$rA, brtarget:$imm),
          !strconcat(mnem, "\t$rA, $imm"), []> {
  let rD = cc;
  let isBranch    = 1;
  let isTerminator = 1;
  let hasDelaySlot = 1;
}

// rD = 0b1_COND (delay bit | 4-bit condition code)
def BEQID : CBranchID<0b10000, "beqid">; // EQ=0
def BNEID : CBranchID<0b10001, "bneid">; // NE=1
def BLTID : CBranchID<0b10010, "bltid">; // LT=2
def BLEID : CBranchID<0b10011, "bleid">; // LE=3
def BGTID : CBranchID<0b10100, "bgtid">; // GT=4
def BGEID : CBranchID<0b10101, "bgeid">; // GE=5

//===----------------------------------------------------------------------===//
// Return from subroutine
//===----------------------------------------------------------------------===//
// RTSD Ra, imm — return with delay slot (opcode=0x2D).
// rD[4:0] encodes the return type; 0b10000 = subroutine return (rtsd).
// Conventional usage: rtsd r15, 8  (delay slot filled by caller or NOP).

def RTSD : MBB<0x2D, (outs), (ins GPR:$rA, simm16:$imm),
               "rtsd\t$rA, $imm", []> {
  let rD = 0b10000;
  let isReturn    = 1;
  let isTerminator = 1;
  let isBarrier   = 1;
  let hasDelaySlot = 1;
  let Uses = [R15];
}

//===----------------------------------------------------------------------===//
// Branch / call / return SelectionDAG patterns
//===----------------------------------------------------------------------===//

// Unconditional branch
def : Pat<(br bb:$dest), (BRI bb:$dest)>;

// Return
def : Pat<(MBRetFlag), (RTSD R15, 8)>;

// Direct call (callee is a global address or external symbol).
def : Pat<(MBCall (i32 tglobaladdr:$callee)), (BRALID R15, tglobaladdr:$callee)>;
def : Pat<(MBCall (i32 texternalsym:$callee)),
          (BRALID R15, texternalsym:$callee)>;


// Global address materialisation via Wrapper → ADDIK rD, r0, symbol.
def : Pat<(MBWrapper tglobaladdr:$in),  (ADDIK R0, tglobaladdr:$in)>;
def : Pat<(MBWrapper texternalsym:$in), (ADDIK R0, texternalsym:$in)>;
def : Pat<(MBWrapper tconstpool:$in),   (ADDIK R0, tconstpool:$in)>;

//===----------------------------------------------------------------------===//
// Conditional select pseudo (SELECT_CC_PSEUDO)
//===----------------------------------------------------------------------===//
// Expanded to a diamond CFG in EmitInstrWithCustomInserter.
// Operands: dst, TrueV, FalseV, CC (ISD::CondCode imm), Diff (LHS-RHS reg).

let usesCustomInserter = 1 in
def SELECT_CC_PSEUDO : MBPseudo<
    (outs GPR:$dst),
    (ins GPR:$T, GPR:$F, i32imm:$CC, GPR:$Diff),
    "#SELECT_CC $dst, $T, $F, $CC, $Diff",
    [(set i32:$dst, (MBSelectCC i32:$T, i32:$F, imm:$CC, i32:$Diff))]>;

// SELECT_CC_CMP_PSEUDO: CMP-based signed conditional select.
// Operands: dst, TrueV, FalseV, CC (ISD::CondCode imm), LHS, RHS.
// EmitInstrWithCustomInserter emits CMP LHS, RHS (bit31=1 iff LHS>RHS)
// and branches using getMBCmpBranchOpcodeForCC (LT↔GT, LE↔GE swapped).
// MBSelectCCCmp: (TrueV, FalseV, CC_imm, LHS, RHS) → result
def SDT_MBSelectCCCmp : SDTypeProfile<1, 5, [
    SDTCisSameAs<0, 1>, SDTCisSameAs<1, 2>,
    SDTCisVT<3, i32>, SDTCisVT<4, i32>, SDTCisVT<5, i32>]>;
def MBSelectCCCmp : SDNode<"MicroBlazeISD::SELECT_CC_CMP", SDT_MBSelectCCCmp, []>;

let usesCustomInserter = 1, Predicates = [HasPatternComparePred] in
def SELECT_CC_CMP_PSEUDO : MBPseudo<
    (outs GPR:$dst),
    (ins GPR:$T, GPR:$F, i32imm:$CC, GPR:$LHS, GPR:$RHS),
    "#SELECT_CC_CMP $dst, $T, $F, $CC, $LHS, $RHS",
    [(set i32:$dst,
      (MBSelectCCCmp i32:$T, i32:$F, imm:$CC, i32:$LHS, i32:$RHS))]>;

// SELECT_CC_CMPU_PSEUDO: CMPU-based unsigned conditional select.
// Operands: dst, TrueV, FalseV, CC (ISD::CondCode imm), LHS, RHS.
// EmitInstrWithCustomInserter emits CMPU LHS, RHS and branches on the result.
// MBSelectCCCmpu: (TrueV, FalseV, CC_imm, LHS, RHS) → result
def SDT_MBSelectCCCmpu : SDTypeProfile<1, 5, [
    SDTCisSameAs<0, 1>, SDTCisSameAs<1, 2>,
    SDTCisVT<3, i32>, SDTCisVT<4, i32>, SDTCisVT<5, i32>]>;
def MBSelectCCCmpu : SDNode<"MicroBlazeISD::SELECT_CC_CMPU", SDT_MBSelectCCCmpu, []>;

// ABS_PSEUDO: integer absolute value expanded by EmitInstrWithCustomInserter.
// Operands: dst, src.
// Diamond: if src > 0, skip negation; fall-through path negates.
//   headMBB:  BGTID src, sinkMBB   (delayed branch; delay slot filled by DSF)
//   negMBB:   RSUBK dst_neg, src, R0   (0 - src = -src; runs only when src ≤ 0)
//   sinkMBB:  dst = phi [dst_neg, negMBB], [src, headMBB]
// DSF's searchJoinBB moves the first safe instruction from sinkMBB into the
// BGTID delay slot, hiding the lw→bgtid load-use stall.
def SDT_MBAbs : SDTypeProfile<1, 1, [SDTCisSameAs<0, 1>, SDTCisVT<0, i32>]>;
def MBAbs : SDNode<"MicroBlazeISD::ABS", SDT_MBAbs, []>;

let usesCustomInserter = 1 in
def ABS_PSEUDO : MBPseudo<
    (outs GPR:$dst),
    (ins GPR:$src),
    "#ABS_PSEUDO $dst, $src",
    [(set i32:$dst, (MBAbs i32:$src))]>;

let usesCustomInserter = 1, Predicates = [HasPatternComparePred] in
def SELECT_CC_CMPU_PSEUDO : MBPseudo<
    (outs GPR:$dst),
    (ins GPR:$T, GPR:$F, i32imm:$CC, GPR:$LHS, GPR:$RHS),
    "#SELECT_CC_CMPU $dst, $T, $F, $CC, $LHS, $RHS",
    [(set i32:$dst,
      (MBSelectCCCmpu i32:$T, i32:$F, imm:$CC, i32:$LHS, i32:$RHS))]>;
