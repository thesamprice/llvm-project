//===-- MicroBlazeDelaySlotFiller.cpp - Fill MicroBlaze delay slots -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// MicroBlaze branches and returns have a one-instruction delay slot (UG984 §2).
// When a taken branch uses a delay slot (the "D" form, e.g. BNED vs BNE), only
// the fetch stage is flushed; the instruction already in the decode stage (the
// delay slot) is allowed to complete before control transfers to the branch
// target.  This reduces the branch penalty from two clock cycles to one.
//
// The generic search / hoist / clone / bundle mechanism lives in
// DelaySlotFillerBase.{h,cpp}; this file supplies the MicroBlaze delay-slot
// policy (the hooks) plus two MicroBlaze-specific pre-fill transforms.
//
// UG984 §2 delay-slot restrictions
// ---------------------------------
// The following instructions are FORBIDDEN in a delay slot:
//
//   IMM / IMML  — An instruction that requires an IMM prefix encodes as two
//                 consecutive 32-bit words.  The delay slot holds exactly one
//                 word, so a two-word instruction there is broken in two ways:
//                 (a) for call delay slots, bralid stores PC+4 past the IMM
//                     word; after the callee returns, the actual instruction
//                     word is silently skipped; (b) for branch delay slots, the
//                 IMM latch leaks into the first instruction at the branch
//                 target.  Enforced by needsMultiWordEncoding()
//                 (needsImmPrefix). (IMML is the 64-bit variant used by MB-X;
//                 not yet in this backend, but needsImmPrefix() would catch it
//                 the same way.)
//
//   Branch      — A branch instruction in a delay slot is architecturally
//                 undefined (UG984 §2).  Enforced by MI.isBranch() in the
//                 shared scan loops; the check also stops the backward scan
//                 from crossing any earlier branch boundary.
//
//   Break       — BRK / BRKI transfer to the break handler; placing one in a
//                 delay slot is architecturally prohibited (UG984 §2).  Both
//                 instructions carry hasSideEffects=1 in their TableGen defs,
//                 which sets MCID::UnmodeledSideEffects; consequently
//                 MI.hasUnmodeledSideEffects() returns true and excludes them
//                 from the shared scan loops.
//
// Recoverable exceptions (e.g. unaligned load/store) ARE allowed in delay slots
// per UG984 §2.  When such an exception fires in a delay slot the hardware sets
// ESR[DS]=1 and stores the branch target address in BTR; the exception handler
// must restart from BTR rather than the usual PC+4.  LLVM does not model this
// special restart path, but the instructions themselves are correctly placed.
//
// Condition evaluation and register hazards (handled by the shared mechanism)
// ---------------------------------------------------------------------------
// The branch evaluates its condition register *before* the delay slot runs, so
// the delay slot may freely write registers that the branch reads — EXCEPT for
// loop backedges where the same branch fires again on the next iteration, and
// EXCEPT for the implicit-use argument/return-value registers of calls/returns.
// The shared searchBackward/searchSuccBBs/searchJoinBB routines implement these
// guards; this file only supplies the target-specific predicates.
//
// Pipeline configurations (3-stage vs 5-stage)
// --------------------------------------------
// Both the C_AREA_OPTIMIZED=0 5-stage (performance) and C_AREA_OPTIMIZED=1
// 3-stage (area) pipelines expose the same architectural delay slot, so the
// filler's placement is pipeline-independent.  What differs is scheduling
// latency (modelled in MicroBlazeSchedule.td: MicroBlazeSpeedModel vs
// MicroBlazeAreaModel) and, potentially, branch-penalty-driven profitability.
// The load-use hook routes through MicroBlazeInstrInfo::isSafeInLoadDelaySlot
// so per-pipeline behavior can live in one place; see hasLoadUseHazard below.
//
//===----------------------------------------------------------------------===//

#include "DelaySlotFillerBase.h"
#include "MicroBlaze.h"
#include "MicroBlazeInstrInfo.h"
#include "MicroBlazeSubtarget.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineInstrBundle.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/InitializePasses.h"

#define GET_INSTRINFO_ENUM
#include "MicroBlazeGenInstrInfo.inc"

#define GET_REGINFO_ENUM
#include "MicroBlazeGenRegisterInfo.inc"

using namespace llvm;

#define DEBUG_TYPE "microblaze-delay-slot-filler"

STATISTIC(HoistedCopies, "Number of ISel copies hoisted into delay slots");
STATISTIC(SunkPreIncrements,
          "Number of loop pre-increments sunk to expose delay slots");

// ---------------------------------------------------------------------------
// MicroBlaze delay-slot policy helpers
// ---------------------------------------------------------------------------

// Return the no-delay equivalent opcode for a delayed branch, or 0 if none
// exists.  Calls (BRALID, BRALD, BRLID, BRLD) and returns (RTSD, RTID, RTBD,
// RTED) have no no-delay form in the MicroBlaze ISA; they always need a slot.
static unsigned getNoDelayVariant(unsigned Opc) {
  switch (Opc) {
  // Unconditional branches — immediate target
  case MicroBlaze::BRID:
    return MicroBlaze::BRI;
  case MicroBlaze::BRAID:
    return MicroBlaze::BRAI;
  // Unconditional branches — register target
  case MicroBlaze::BRD:
    return MicroBlaze::BR_;
  case MicroBlaze::BRAD:
    return MicroBlaze::BRA;
  // Conditional branches — immediate target
  case MicroBlaze::BEQID:
    return MicroBlaze::BEQI;
  case MicroBlaze::BNEID:
    return MicroBlaze::BNEI;
  case MicroBlaze::BLTID:
    return MicroBlaze::BLTI;
  case MicroBlaze::BLEID:
    return MicroBlaze::BLEI;
  case MicroBlaze::BGTID:
    return MicroBlaze::BGTI;
  case MicroBlaze::BGEID:
    return MicroBlaze::BGEI;
  // Conditional branches — register target
  case MicroBlaze::BEQD:
    return MicroBlaze::BEQ;
  case MicroBlaze::BNED:
    return MicroBlaze::BNE;
  case MicroBlaze::BLTD:
    return MicroBlaze::BLT;
  case MicroBlaze::BLED:
    return MicroBlaze::BLE;
  case MicroBlaze::BGTD:
    return MicroBlaze::BGT;
  case MicroBlaze::BGED:
    return MicroBlaze::BGE;
  default:
    return 0;
  }
}

// True if MI would need a preceding IMM prefix word when encoded.
// UG984 §2 explicitly forbids IMM (and its 64-bit variant IMML) in a delay
// slot.  Beyond the architectural prohibition, a two-word encoding is
// structurally broken in a one-word slot: for call delay slots the bralid
// return address points past the IMM word, so the actual instruction word is
// silently skipped after the callee returns; for branch delay slots the IMM
// latch leaks into the first instruction at the branch target.
static bool needsImmPrefix(const MachineInstr &MI) {
  for (const MachineOperand &MO : MI.operands()) {
    switch (MO.getType()) {
    case MachineOperand::MO_GlobalAddress:
    case MachineOperand::MO_ExternalSymbol:
    case MachineOperand::MO_ConstantPoolIndex:
    case MachineOperand::MO_JumpTableIndex:
    case MachineOperand::MO_BlockAddress:
      return true;
    case MachineOperand::MO_Immediate:
      if (MO.getImm() < -32768 || MO.getImm() > 32767)
        return true;
      break;
    default:
      break;
    }
  }
  return false;
}

// ---------------------------------------------------------------------------
// Pre-fill transform: copy-to-delay-slot hoisting
// ---------------------------------------------------------------------------
//
// Recognises:
//   addk  rX, rY, R0       -- rX = rY  (ISel register copy; rX != rY)
//   [zero or more instructions that neither define rX nor define rY]
//   <D-form conditional branch targeting a backward block>
//
// and transforms to:
//   [intervening instructions — every USE of rX replaced by rY]
//   <D-form branch, bundled with addk as its delay slot>
//   addk  rX, rY, R0       -- [bundle body: executes as the delay slot]
//
// Correctness: the delay slot fires unconditionally (taken and fall-through
// paths alike), so rX = rY before any successor block begins — identical to
// the original semantics.  Any use of rX in an intermediate instruction is
// equivalent to rY because rY is unmodified in the scan range.
//
// The DSF main loop skips pre-bundled branches via:
//   if (I->isBundledWithSucc()) continue;
static bool hoistCopyToDelaySlot(MachineBasicBlock &MBB) {
  bool Changed = false;

  for (auto I = MBB.begin(), E = MBB.end(); I != E;) {
    // Match: addk rX, rY, R0
    if (I->getOpcode() != MicroBlaze::ADDK) {
      ++I;
      continue;
    }
    Register rX = I->getOperand(0).getReg();
    Register rY = I->getOperand(1).getReg();
    if (I->getOperand(2).getReg() != MicroBlaze::R0 || rX == rY) {
      ++I;
      continue;
    }

    auto Addk = I++;
    // I now points to the first instruction after Addk.

    // Scan forward for a D-form backward branch.  Abort if rX or rY is
    // redefined, or if a call/return is encountered.
    MachineBasicBlock::iterator BranchIt = E;

    for (auto J = I; J != E; ++J) {
      // Abort if any instruction redefines rX or rY.
      bool Abort = false;
      for (const MachineOperand &MO : J->operands()) {
        if (!MO.isReg() || !MO.isDef())
          continue;
        if (MO.getReg() == rX || MO.getReg() == rY) {
          Abort = true;
          break;
        }
      }
      if (Abort)
        break;

      if (J->isCall() || J->isReturn())
        break;

      if (J->isBranch()) {
        // Only match an unoccupied D-form backward branch: taken nearly every
        // iteration, so D-form + filled slot (2 cycles) beats a NOP or
        // demotion to non-D (3 cycles taken).  Skip already-bundled branches
        // to avoid extending a 1-slot bundle into a multi-slot bundle.
        if (J->hasDelaySlot() && !J->isBundledWithSucc()) {
          for (const MachineOperand &MO : J->operands()) {
            if (MO.isMBB() && MO.getMBB()->getNumber() <= MBB.getNumber()) {
              BranchIt = J;
              break;
            }
          }
        }
        break;
      }
    }

    if (BranchIt == E)
      continue;

    // Substitute every USE of rX with rY in [Addk+1 .. BranchIt] inclusive.
    // The branch is included so that a branch that directly tests rX (e.g.
    // bgeid rX) is correctly rewritten to test rY.  The addk operand is a def
    // and is excluded by the !MO.isDef() guard.
    for (auto K = I; K != std::next(BranchIt); ++K)
      for (MachineOperand &MO : K->operands())
        if (MO.isReg() && !MO.isDef() && MO.getReg() == rX)
          MO.setReg(rY);

    // Move Addk into the delay slot: splice immediately after BranchIt, then
    // bundle branch + Addk together (mirrors searchBackward's splice+bundle).
    MBB.splice(std::next(BranchIt), &MBB, Addk);
    MIBundleBuilder(MBB, BranchIt.getInstrIterator(),
                    std::next(BranchIt.getInstrIterator(), 2));
    ++HoistedCopies;
    Changed = true;
    // I was already advanced past Addk; continue scanning from there.
  }
  return Changed;
}

// ---------------------------------------------------------------------------
// Pre-fill transform: pre-increment sinking
// ---------------------------------------------------------------------------
//
// ISel copies the pointer register before advancing it so the old address
// is still available for the subsequent load.  Two orderings appear in
// practice depending on which direction the scan advances:
//
// Pattern A — advance-before-load (left scan, positive stride):
//   addk  rA, rB, R0      -- rA = rB  (save old pointer)
//   addik rB, rA, N       -- rB = rB + N  (advance; N > 0)
//   lwi   rC, rA, OFF     -- rC = *(rB_old + OFF)  (load via saved pointer)
//
// Pattern B — load-before-advance (right scan, negative stride):
//   addk  rA, rB, R0      -- rA = rB  (save old pointer)
//   lwi   rC, rA, OFF     -- rC = *(rB_old + OFF)  (load via saved pointer)
//   addik rB, rA, N       -- rB = rB + N  (advance; N < 0)
//
// Both patterns share the same fix: rA is unnecessary.  Load directly from
// rB using the same offset, then advance rB.  The load address is unchanged
// because rB still holds the old pointer at load time.
//
//   lwi   rC, rB, OFF     -- rC = *(rB_old + OFF)  same address
//   addik rB, rB, N       -- rB = rB + N            same result
//
// Constraint: rC must not equal rB (otherwise lwi clobbers the base before
// addik uses it).  In Pattern A, rC is often rA (≠ rB by construction).
//
// After sinking addik to immediately before the backward branch,
// searchBackward moves it into the delay slot automatically.  The exit path
// compensates for the unconditional execution of addik in the slot.
static bool sinkPreIncrements(MachineBasicBlock &MBB) {
  bool Changed = false;

  for (auto I = MBB.begin(), E = MBB.end(); I != E;) {
    // Match: addk rA, rB, R0
    if (I->getOpcode() != MicroBlaze::ADDK) {
      ++I;
      continue;
    }
    Register rA = I->getOperand(0).getReg();
    Register rB = I->getOperand(1).getReg();
    if (I->getOperand(2).getReg() != MicroBlaze::R0 || rA == rB) {
      ++I;
      continue;
    }

    auto Addk = I++;
    if (I == E)
      break;

    MachineBasicBlock::iterator Addik, Lwi;
    bool PatternA = false;

    if (I->getOpcode() == MicroBlaze::ADDIK &&
        I->getOperand(0).getReg() == rB && I->getOperand(1).getReg() == rA) {
      // Pattern A: addik rB, rA, N follows the addk.
      Addik = I++;
      if (I == E)
        break;
      if (I->getOpcode() != MicroBlaze::LWI ||
          I->getOperand(1).getReg() != rA) {
        continue;
      }
      Lwi = I++;
      PatternA = true;
    } else if (I->getOpcode() == MicroBlaze::LWI &&
               I->getOperand(1).getReg() == rA) {
      // Pattern B: lwi rC, rA, OFF follows the addk.
      Lwi = I++;
      if (I == E)
        break;
      if (I->getOpcode() != MicroBlaze::ADDIK ||
          I->getOperand(0).getReg() != rB || I->getOperand(1).getReg() != rA) {
        continue;
      }
      Addik = I++;
    } else {
      continue;
    }

    // Safety: lwi destination must not alias rB (would clobber the pointer
    // base before addik reads it in the lwi-before-addik order).
    Register rC = Lwi->getOperand(0).getReg();
    if (rC == rB)
      continue;

    // Rewrite both instructions to use rB directly, eliminating rA.
    Lwi->getOperand(1).setReg(rB);   // lwi rC, rA, OFF → lwi rC, rB, OFF
    Addik->getOperand(1).setReg(rB); // addik rB, rA, N → addik rB, rB, N

    if (PatternA) {
      // Reorder: move lwi to before addik.
      MBB.splice(Addik, &MBB, Lwi);
    }
    // Pattern B order is already lwi → addik; no splice needed.

    // Remove the now-redundant addk copy.
    Addk->eraseFromParent();
    // Block now: ... Lwi → Addik → I ...

    // Sink addik past non-rB instructions until the first rB user/def or
    // terminator, placing it immediately before the backward branch so that
    // searchBackward can move it into the delay slot.
    while (I != E && !I->isTerminator()) {
      bool TouchesRB =
          llvm::any_of(I->operands(), [rB](const MachineOperand &MO) {
            return MO.isReg() && MO.getReg() == rB;
          });
      if (TouchesRB)
        break;
      ++I;
    }
    if (std::next(Addik) != I)
      MBB.splice(I, &MBB, Addik);

    ++SunkPreIncrements;
    Changed = true;
  }
  return Changed;
}

// ---------------------------------------------------------------------------
// MicroBlaze delay-slot filler pass
// ---------------------------------------------------------------------------

namespace {

class MicroBlazeDelaySlotFiller : public DelaySlotFiller {
public:
  static char ID;

  MicroBlazeDelaySlotFiller() : DelaySlotFiller(ID) {
    initializeMicroBlazeDelaySlotFillerPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override {
    return "MicroBlaze delay slot filler";
  }

protected:
  bool needsMultiWordEncoding(const MachineInstr &MI) const override {
    return needsImmPrefix(MI);
  }

  unsigned getNoDelayOpcode(unsigned Opc) const override {
    return getNoDelayVariant(Opc);
  }

  void insertNop(MachineBasicBlock &MBB, MachineBasicBlock::iterator Pos,
                 const DebugLoc &DL) const override {
    BuildMI(MBB, Pos, DL, TII->get(MicroBlaze::NOP));
  }

  bool isLoadInstruction(const MachineInstr &MI) const override {
    return static_cast<const MicroBlazeInstrInfo *>(TII)->isLoadInstruction(MI);
  }

  // UG984 §2 / IIC_LD=2: MicroBlaze has a 2-cycle load-to-use latency, but the
  // branch occupies the pipeline stage between the load and its delay slot, so
  // the result is always ready when the filler reads it — no load-use hazard on
  // either the 5-stage or the 3-stage pipeline.  Routed through the target
  // predicate so the invariant stays testable and pipeline-selectable in one
  // place (isSafeInLoadDelaySlot may consult hasAreaOptimized() in future).
  bool hasLoadUseHazard(const MachineInstr &Filler,
                        const MachineInstr &Load) const override {
    return !static_cast<const MicroBlazeInstrInfo *>(TII)
                ->isSafeInLoadDelaySlot(Filler, Load);
  }

  // When no filler is found, choose keep-D+NOP vs demote-to-non-D.
  //
  // The base heuristic keeps the D-form + a NOP for backward (taken-heavy loop
  // back-edge) branches and demotes forward ones.  With a branch target cache
  // (UG984 Ch.2 "Branch Target Cache"), a correctly predicted immediate branch
  // has its refill hidden, so the D-form saves nothing and the NOP is a wasted
  // cycle every iteration — demote instead.  A backward branch always has an
  // immediate (MBB) target, which the BTC predicts; register/indirect branches
  // (never predicted) have no MBB operand, so IsBackward is false for them and
  // they keep the base behavior.  The BTC does not exist on the 3-stage area
  // pipeline, so the !hasAreaOptimized() guard ignores that impossible combo.
  bool preferNopOverDemote(const MachineInstr &Br,
                           bool IsBackward) const override {
    const auto &STI = Br.getMF()->getSubtarget<MicroBlazeSubtarget>();
    if (IsBackward && STI.hasBranchTargetCache() && !STI.hasAreaOptimized())
      return false; // BTC hides the refill → demote to the no-delay form
    return DelaySlotFiller::preferNopOverDemote(Br, IsBackward);
  }

  // MicroBlaze-specific transforms that expose delay-slot opportunities.
  bool runPreFillTransforms(MachineFunction &MF) override {
    bool Changed = false;
    for (MachineBasicBlock &MBB : MF)
      Changed |= hoistCopyToDelaySlot(MBB);
    for (MachineBasicBlock &MBB : MF)
      Changed |= sinkPreIncrements(MBB);
    return Changed;
  }
};

} // namespace

char MicroBlazeDelaySlotFiller::ID = 0;

INITIALIZE_PASS_BEGIN(MicroBlazeDelaySlotFiller, DEBUG_TYPE,
                      "MicroBlaze delay slot filler", false, false)
INITIALIZE_PASS_DEPENDENCY(MachineBranchProbabilityInfoWrapperPass)
INITIALIZE_PASS_END(MicroBlazeDelaySlotFiller, DEBUG_TYPE,
                    "MicroBlaze delay slot filler", false, false)

FunctionPass *llvm::createMicroBlazeDelaySlotFiller() {
  return new MicroBlazeDelaySlotFiller();
}
