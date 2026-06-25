//===-- MicroBlazeRegisterInfo.td - MicroBlaze Register Defs -*-tablegen-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

class MBReg<bits<5> num, string n, list<string> altNames = []>
    : Register<n, altNames> {
  field bits<5> Num = num;
  let Namespace = "MicroBlaze";
  let HWEncoding{4-0} = num;
}

// DWARF register numbers match the hardware register numbers for MicroBlaze.
foreach i = 0-31 in {
  def R#i : MBReg<i, "r"#i>, DwarfRegNum<[i]>;
}


//===----------------------------------------------------------------------===//
// Register classes
//===----------------------------------------------------------------------===//

// GPR — all 32 integer registers.  Reserved registers (R0, R1, R2, R13–R18)
// are excluded from allocation by getReservedRegs(); the assembler still
// accepts them so that asm code can reference r0 (the zero register), r1
// (stack pointer), etc.  Allocation order: caller-saved first, callee-saved
// last; reserved registers at the back so the allocator avoids them naturally.
// Callee-saved registers per MicroBlaze ABI: R22–R31.
def CSR : CalleeSavedRegs<(add R22, R23, R24, R25, R26, R27, R28, R29, R30, R31)>;

def GPR : RegisterClass<"MicroBlaze", [i32], 32, (add
    // Return values / argument registers / caller-saved scratch
    R3, R4, R5, R6, R7, R8, R9, R10, R11, R12,
    // Frame pointer (eliminable to R1), PIC GOT pointers
    R19, R20, R21,
    // Callee-saved
    R22, R23, R24, R25, R26, R27, R28, R29, R30, R31,
    // Reserved — available for asm but not allocated by the register allocator
    R0, R1, R2, R13, R14, R15, R16, R17, R18
)>;
