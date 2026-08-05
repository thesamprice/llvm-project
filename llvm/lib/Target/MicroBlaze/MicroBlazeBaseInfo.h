//===-- MicroBlazeBaseInfo.h - MicroBlaze target operand flags ---*- C++
//-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEBASEINFO_H
#define LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEBASEINFO_H

namespace llvm {
namespace MicroBlazeII {

// Target operand flags for MachineOperand::getTargetFlags().
// These differentiate how a symbol reference should be encoded.
enum TOF : unsigned {
  MO_NO_FLAG = 0,
  MO_GOT = 1, // @got: load address from GOT via R20 (R_MICROBLAZE_GOT_64)
  MO_PLT = 2, // @plt: call via PLT stub (R_MICROBLAZE_PLT_64)
  MO_GOTOFF =
      3, // @gotoff: link-time offset from the GOT base (R_MICROBLAZE_GOTOFF_64)
};

} // namespace MicroBlazeII
} // namespace llvm

#endif // LLVM_LIB_TARGET_MICROBLAZE_MICROBLAZEBASEINFO_H
