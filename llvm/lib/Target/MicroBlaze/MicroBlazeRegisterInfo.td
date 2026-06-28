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

// DWARF register numbers match hardware register numbers.
// Architectural alt-names come from UG984: R0=zero, R1=sp, R15=lr, R19=fp.
def R0  : MBReg< 0, "r0",  ["zero"]>, DwarfRegNum<[0]>;
def R1  : MBReg< 1, "r1",  ["sp"]>,   DwarfRegNum<[1]>;
def R2  : MBReg< 2, "r2">,            DwarfRegNum<[2]>;
def R3  : MBReg< 3, "r3">,            DwarfRegNum<[3]>;
def R4  : MBReg< 4, "r4">,            DwarfRegNum<[4]>;
def R5  : MBReg< 5, "r5">,            DwarfRegNum<[5]>;
def R6  : MBReg< 6, "r6">,            DwarfRegNum<[6]>;
def R7  : MBReg< 7, "r7">,            DwarfRegNum<[7]>;
def R8  : MBReg< 8, "r8">,            DwarfRegNum<[8]>;
def R9  : MBReg< 9, "r9">,            DwarfRegNum<[9]>;
def R10 : MBReg<10, "r10">,           DwarfRegNum<[10]>;
def R11 : MBReg<11, "r11">,           DwarfRegNum<[11]>;
def R12 : MBReg<12, "r12">,           DwarfRegNum<[12]>;
def R13 : MBReg<13, "r13">,           DwarfRegNum<[13]>;
def R14 : MBReg<14, "r14">,           DwarfRegNum<[14]>;
def R15 : MBReg<15, "r15", ["lr"]>,   DwarfRegNum<[15]>;
def R16 : MBReg<16, "r16">,           DwarfRegNum<[16]>;
def R17 : MBReg<17, "r17">,           DwarfRegNum<[17]>;
def R18 : MBReg<18, "r18">,           DwarfRegNum<[18]>;
def R19 : MBReg<19, "r19", ["fp"]>,   DwarfRegNum<[19]>;
def R20 : MBReg<20, "r20">,           DwarfRegNum<[20]>;
def R21 : MBReg<21, "r21">,           DwarfRegNum<[21]>;
def R22 : MBReg<22, "r22">,           DwarfRegNum<[22]>;
def R23 : MBReg<23, "r23">,           DwarfRegNum<[23]>;
def R24 : MBReg<24, "r24">,           DwarfRegNum<[24]>;
def R25 : MBReg<25, "r25">,           DwarfRegNum<[25]>;
def R26 : MBReg<26, "r26">,           DwarfRegNum<[26]>;
def R27 : MBReg<27, "r27">,           DwarfRegNum<[27]>;
def R28 : MBReg<28, "r28">,           DwarfRegNum<[28]>;
def R29 : MBReg<29, "r29">,           DwarfRegNum<[29]>;
def R30 : MBReg<30, "r30">,           DwarfRegNum<[30]>;
def R31 : MBReg<31, "r31">,           DwarfRegNum<[31]>;

//===----------------------------------------------------------------------===//
// Register classes
//===----------------------------------------------------------------------===//

// Callee-saved registers per MicroBlaze ABI: R19 (frame pointer), R22–R31.
def CSR : CalleeSavedRegs<(add R19, R22, R23, R24, R25, R26, R27, R28, R29, R30, R31)>;

// GPR — all potentially allocatable integer registers.  Reserved registers
// (R0, R1, R2, R13, R14, R15, R16, R17, R18) are excluded from this class;
// they are handled in MicroBlazeRegisterInfo::getReservedRegs().
// R17 is reserved unconditionally because hardware exceptions are assumed.
// Allocation order: return/arg/caller-saved first, then callee-saved.
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

// FPR — same physical registers as GPR, typed as f32.  MicroBlaze hardware
// floating-point instructions (opcode 0x16, UG984 §5) operate on general-
// purpose registers, interpreting their bits as IEEE 754 single-precision
// values.  A separate register class is required so that SelectionDAG can
// type-check f32 operands and allocate them through the same physical file.
def FPR : RegisterClass<"MicroBlaze", [f32], 32, (add
    R3, R4, R5, R6, R7, R8, R9, R10, R11, R12,
    R19, R20, R21,
    R22, R23, R24, R25, R26, R27, R28, R29, R30, R31,
    R0, R1, R2, R13, R14, R15, R16, R17, R18
)>;

//===----------------------------------------------------------------------===//
// FSL port "registers" rfsl0-rfsl15
// Used as static FSL get/put operands (opcode 0x1B, bits[7:4]).
// HWEncoding is the 4-bit port number placed in instruction bits[7:4].
// The RFSL register class is not allocatable — rfslN appear only in asm.
//===----------------------------------------------------------------------===//

foreach I = 0-15 in {
  def rfsl#I : Register<"rfsl"#I> {
    let Namespace = "MicroBlaze";
    let HWEncoding = I;
  }
}

def RFSL : RegisterClass<"MicroBlaze", [i32], 32, (add
    rfsl0,  rfsl1,  rfsl2,  rfsl3,
    rfsl4,  rfsl5,  rfsl6,  rfsl7,
    rfsl8,  rfsl9,  rfsl10, rfsl11,
    rfsl12, rfsl13, rfsl14, rfsl15)> {
  let Size = 32;
  let isAllocatable = 0;
}

//===----------------------------------------------------------------------===//
// MSR.C — Machine Status Register carry bit (bit 29)
//
// Modeled as a virtual 32-bit register so the SelectionDAG can track carry
// flow between ADD/ADDC/RSUB/RSUBC instructions without threading it through
// a GPR.  The register is NOT allocatable; it only appears as an implicit
// Def/Use on the four carry-arithmetic instructions, and as the explicit
// third operand / second result in the MicroBlazeISD::ADDE/SUBE DAG nodes.
//===----------------------------------------------------------------------===//

def MSR_C : Register<"msr.c"> {
  let Namespace = "MicroBlaze";
  // No DWARF number — not visible to debuggers.
}

def MSRCRC : RegisterClass<"MicroBlaze", [i32], 32, (add MSR_C)> {
  let isAllocatable = 0;
}
