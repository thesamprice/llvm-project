//===-- MicroBlazeInstrBranch2.td - MicroBlaze Instruction Definitions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// Unconditional branches with register target (Type A, opcode 0x26)
// UG984 §5: rA modifier field = {delay[4], absolute[3], link[2], 0, 0}
// Latency: 1 cycle (not taken); 2–7 cycles (taken, depends on D bit).
//===----------------------------------------------------------------------===//

// BR rB — PC-relative branch to rB, no delay  (rD=0, rA=0b00000).
def BR_   : MBA<0x26, 0, (outs), (ins GPR:$rB),
                "br\t$rB", []> {
  let rD = 0b00000;
  let rA = 0b00000;
  let isBranch = 1; let isTerminator = 1; let isBarrier = 1;
}

// BRD rB — PC-relative, delay slot  (rA=0b10000).
def BRD   : MBA<0x26, 0, (outs), (ins GPR:$rB),
                "brd\t$rB", []> {
  let rD = 0b00000;
  let rA = 0b10000;
  let isBranch = 1; let isTerminator = 1; let isBarrier = 1; let hasDelaySlot = 1;
}

// BRA rB — absolute branch (rA=0b01000).
def BRA   : MBA<0x26, 0, (outs), (ins GPR:$rB),
                "bra\t$rB", []> {
  let rD = 0b00000;
  let rA = 0b01000;
  let isBranch = 1; let isTerminator = 1; let isBarrier = 1;
}

// BRAD rB — absolute branch, delay  (rA=0b11000).
// Selection target for ISD::BRIND (computed goto / indirect branch through a
// register-held absolute address, e.g. a blockaddress or jump-table entry).
def BRAD  : MBA<0x26, 0, (outs), (ins GPR:$rB),
                "brad\t$rB", []> {
  let rD = 0b00000;
  let rA = 0b11000;
  let isBranch = 1; let isTerminator = 1; let isBarrier = 1;
  let isIndirectBranch = 1; let hasDelaySlot = 1;
}

// Indirect branch: target absolute address held in a GPR.
def : Pat<(brind GPR:$rB), (BRAD GPR:$rB)>;

// BRLD rD, rB — relative branch w/ link and delay; rD ← PC+4  (rA=0b10100).
def BRLD  : MBA<0x26, 0, (outs), (ins GPR:$rD, GPR:$rB),
                "brld\t$rD, $rB", []> {
  let rA = 0b10100;
  let isCall = 1; let hasDelaySlot = 1;
}

// BRALD rD, rB — absolute branch w/ link and delay; rD ← PC+4  (rA=0b11100).
// Used for indirect calls (function pointer in a register).
def BRALD : MBA<0x26, 0, (outs), (ins GPR:$rD, GPR:$rB),
                "brald\t$rD, $rB", []> {
  let rA = 0b11100;
  let isCall = 1; let hasDelaySlot = 1; let Defs = [R15];
}

// Indirect call: callee is in a GPR (function pointer or any non-direct target).
// LowerCall copies any non-GlobalAddress/non-ExternalSymbol callee into a
// virtual register, so this pattern covers all indirect-call cases.
def : Pat<(MBCall GPR:$callee), (BRALD R15, GPR:$callee)>;

// BRK rD, rB — software break; enters break handler; rD ← PC+4  (rA=0b01100).
// Control transfers to the break handler and never returns to the next
// instruction, so isBarrier=1.  hasSideEffects=1 serves double duty: it
// models the architectural side effects (writes BTR and sets MSR.BIP), and
// it sets MCID::UnmodeledSideEffects so that the delay slot filler's
// hasUnmodeledSideEffects() check correctly excludes BRK from delay slots
// per UG984 §2 (break instructions are forbidden in delay slots).
def BRK   : MBA<0x26, 0, (outs), (ins GPR:$rD, GPR:$rB),
                "brk\t$rD, $rB", []> {
  let rA = 0b01100;
  let isTerminator = 1;
  let isBarrier = 1;
  let hasSideEffects = 1;
}

//===----------------------------------------------------------------------===//
// Conditional branches — register target (Type A, opcode 0x27)
// UG984 §5: rD[4]=D(delay), rD[3:0]=condition;  rA=test register;  rB=target.
// Latency: 1 cycle (not taken); 2–7 cycles (taken), same as beq latency table.
//===----------------------------------------------------------------------===//

class CBranchR<bits<5> cc, string mnem>
    : MBA<0x27, 0, (outs), (ins GPR:$rA, GPR:$rB),
          !strconcat(mnem, "\t$rA, $rB"), []> {
  let rD = cc;
  let isBranch = 1; let isTerminator = 1;
}

class CBranchRD<bits<5> cc, string mnem>
    : MBA<0x27, 0, (outs), (ins GPR:$rA, GPR:$rB),
          !strconcat(mnem, "\t$rA, $rB"), []> {
  let rD = cc;
  let isBranch = 1; let isTerminator = 1; let hasDelaySlot = 1;
}

// Non-delay (D=0) conditional register branches
def BEQ   : CBranchR<0b00000, "beq">;   // EQ
def BNE   : CBranchR<0b00001, "bne">;   // NE
def BLT   : CBranchR<0b00010, "blt">;   // LT
def BLE   : CBranchR<0b00011, "ble">;   // LE
def BGT   : CBranchR<0b00100, "bgt">;   // GT
def BGE   : CBranchR<0b00101, "bge">;   // GE

// Delay-slot (D=1) conditional register branches
def BEQD  : CBranchRD<0b10000, "beqd">;
def BNED  : CBranchRD<0b10001, "bned">;
def BLTD  : CBranchRD<0b10010, "bltd">;
def BLED  : CBranchRD<0b10011, "bled">;
def BGTD  : CBranchRD<0b10100, "bgtd">;
def BGED  : CBranchRD<0b10101, "bged">;

//===----------------------------------------------------------------------===//
// Missing Type B unconditional branches (opcode 0x2E, UG984 §5)
// BRAI (absolute no delay), BRLID (relative link delay), BRKI (break imm).
//===----------------------------------------------------------------------===//

// BRAI imm — absolute branch to immediate, no delay.  A-flag in rA[20:16].
def BRAI  : MBB<0x2E, (outs), (ins calltarget:$imm), "brai\t$imm", []> {
  let rD = 0b00000;
  let rA = 0b01000;  // A at bit19; matches QEMU insns.decode and GAS
  let isBranch = 1; let isTerminator = 1; let isBarrier = 1;
}

// BRLID rD, imm — relative branch link delay; rD ← PC+4  (rA=0b10100).
def BRLID : MBB<0x2E, (outs), (ins GPR:$rD, brtarget:$imm),
                "brlid\t$rD, $imm", []> {
  let rA = 0b10100;
  let isCall = 1; let hasDelaySlot = 1; let Defs = [R15];
}

// BRKI rD, imm — break to immediate; rD ← PC+4  (rA=0b01100).
// Same semantics as BRK: transfers to break handler, never falls through.
// hasSideEffects=1 and the calltarget immediate (→ MO_GlobalAddress, caught by
// needsImmPrefix) doubly exclude BRKI from delay slots per UG984 §2.
def BRKI  : MBB<0x2E, (outs), (ins GPR:$rD, calltarget:$imm),
                "brki\t$rD, $imm", []> {
  let rA = 0b01100;
  let isTerminator = 1;
  let isBarrier = 1;
  let hasSideEffects = 1;
}

//===----------------------------------------------------------------------===//
// Conditional branches — immediate target, no delay (Type B, opcode 0x2F)
// rD[4:0] = 0_COND (D=0).  Complements the BEQID..BGEID delay-slot forms.
// UG984 §5.  Latency: 1 cycle (not taken); 2–7 cycles (taken).
//===----------------------------------------------------------------------===//

class CBranchNID<bits<5> cc, string mnem>
    : MBB<0x2F, (outs), (ins GPR:$rA, brtarget:$imm),
          !strconcat(mnem, "\t$rA, $imm"), []> {
  let rD = cc;
  let isBranch = 1; let isTerminator = 1;
}

def BEQI  : CBranchNID<0b00000, "beqi">;   // EQ, no delay
def BNEI  : CBranchNID<0b00001, "bnei">;   // NE
def BLTI  : CBranchNID<0b00010, "blti">;   // LT
def BLEI  : CBranchNID<0b00011, "blei">;   // LE
def BGTI  : CBranchNID<0b00100, "bgti">;   // GT
def BGEI  : CBranchNID<0b00101, "bgei">;   // GE

//===----------------------------------------------------------------------===//
// __builtin_setjmp / __builtin_longjmp pseudo instructions
// Expanded by EmitInstrWithCustomInserter into a 4-MBB diamond.
//
// EH_SjLj_SetJmp: saves FP→buf[0], resumes_addr→buf[1], SP→buf[2]; returns 0/1.
// EH_SjLj_LongJmp: loads FP←buf[0], SP←buf[2], jumps to buf[1].
//===----------------------------------------------------------------------===//

// SDTypeProfile counts only non-chain types; chain I/O is implicit via SDNPHasChain.
// EH_SJLJ_SETJMP(chain, buf_ptr) → (i32, chain, glue): 1 output, 1 input.
// EH_SJLJ_LONGJMP(chain, buf_ptr) → chain: 0 outputs, 1 input.
def SDT_MBSjLjSetJmp  : SDTypeProfile<1, 1,
    [SDTCisVT<0, i32>, SDTCisPtrTy<1>]>;
def SDT_MBSjLjLongJmp : SDTypeProfile<0, 1,
    [SDTCisPtrTy<0>]>;

def MBSjLjSetJmp  : SDNode<"MicroBlazeISD::EH_SJLJ_SETJMP",
    SDT_MBSjLjSetJmp, [SDNPHasChain, SDNPSideEffect, SDNPOutGlue]>;
def MBSjLjLongJmp : SDNode<"MicroBlazeISD::EH_SJLJ_LONGJMP",
    SDT_MBSjLjLongJmp, [SDNPHasChain, SDNPSideEffect]>;

let hasSideEffects = 1, isCodeGenOnly = 1, usesCustomInserter = 1 in {
  def EH_SjLj_SetJmp  : MBPseudo<
      (outs GPR:$dst),
      (ins GPR:$buf),
      "# EH_SJLJ_SETJMP $dst, $buf",
      [(set i32:$dst, (MBSjLjSetJmp iPTR:$buf))]>;

  let isTerminator = 1, isBarrier = 1 in
  def EH_SjLj_LongJmp : MBPseudo<
      (outs),
      (ins GPR:$buf),
      "# EH_SJLJ_LONGJMP $buf",
      [(MBSjLjLongJmp iPTR:$buf)]>;
}
