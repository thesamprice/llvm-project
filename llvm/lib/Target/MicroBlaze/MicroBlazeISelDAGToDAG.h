//===-- MicroBlazeISelDAGToDAG.h - MicroBlaze DAG→DAG selector --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEISELDAGTODAG_H
#define LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEISELDAGTODAG_H

#include "MCTargetDesc/MicroBlazeMCTargetDesc.h"
#include "MicroBlazeSubtarget.h"
#include "MicroBlazeTargetMachine.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/SelectionDAGISel.h"

namespace llvm {

// MicroBlazeDAGToDAGISel — SelectionDAG → MachineInstr selector.
// The static char ID and MachineFunctionPass machinery lives in the
// MicroBlazeDAGToDAGISelLegacy wrapper below.
class MicroBlazeDAGToDAGISel : public SelectionDAGISel {
  // Set at the start of each function; referenced by generated
  // CheckPatternPredicate.
  const MicroBlazeSubtarget *Subtarget = nullptr;

public:
  MicroBlazeDAGToDAGISel() = delete;
  explicit MicroBlazeDAGToDAGISel(MicroBlazeTargetMachine &TM,
                                  CodeGenOptLevel OptLevel);

  StringRef getPassName() const {
    return "MicroBlaze DAG->DAG Pattern Instruction Selection";
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    Subtarget = &MF.getSubtarget<MicroBlazeSubtarget>();
    return SelectionDAGISel::runOnMachineFunction(MF);
  }

  // Called from ComplexPattern predicates in InstrInfo.td.
  bool SelectADDRri(SDValue Addr, SDValue &Base, SDValue &Offset);
  bool SelectADDRrr(SDValue Addr, SDValue &Base, SDValue &Index);

  // Match a constant FSL port number in 0..15 and bind it to the corresponding
  // non-allocatable rfslN register, so a static FSL get/put can encode the port
  // in the instruction word.  Fails for non-constants and out-of-range values,
  // which then select the dynamic (port-in-GPR) form.  See
  // MicroBlazeInstrFSL.td.
  bool SelectFSLImm(SDValue N, SDValue &Port);

  bool tryBSEFI(SDNode *N);
  bool tryBSIFI(SDNode *N);

  void Select(SDNode *Node) override;

// Auto-generated instruction selector (SelectCode, CheckPatternPredicate, …).
// Included inside the class so generated inline functions can access members.
// Opcode names are visible because MicroBlazeMCTargetDesc.h is included above.
#define GET_DAGISEL_DECL
#include "MicroBlazeGenDAGISel.inc"
#undef GET_DAGISEL_DECL
};

// Legacy MachineFunctionPass wrapper required by the pre-NPM pass manager.
class MicroBlazeDAGToDAGISelLegacy : public SelectionDAGISelLegacy {
public:
  static char ID;
  explicit MicroBlazeDAGToDAGISelLegacy(MicroBlazeTargetMachine &TM,
                                        CodeGenOptLevel OptLevel);
};

FunctionPass *createMicroBlazeISelDag(MicroBlazeTargetMachine &TM,
                                      CodeGenOptLevel OptLevel);

} // namespace llvm

#endif // LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEISELDAGTODAG_H
