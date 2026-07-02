//===-- MicroBlazeInstrMem.td - MicroBlaze Instruction Definitions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// mayLoad/mayStore must be set explicitly on each group: MachineInstr::mayLoad()
// and mayStore() only check the MCInstrDesc flag; they do not fall back to
// MachineMemOperands. Our patterns are defined separately via Pat<>, so
// TableGen cannot infer these flags from the instruction definition alone.

//===----------------------------------------------------------------------===//
// Load instructions — immediate offset (Type B)
//===----------------------------------------------------------------------===//

// Load instructions use a combined memri/memrr address operand so that the
// ADDRri/ADDRrr ComplexPatterns can match them directly.
// The 21-bit addr field encodes [20:16]=base register and [15:0]=imm offset.

let mayLoad = 1, hasSideEffects = 0 in {

// LWI  Rd, Ra, imm — load word     (opcode=0x3A)
def LWI  : MBB<0x3A, (outs GPR:$rD), (ins memri:$addr),
               "lwi\t$rD, $addr", []> {
  bits<21> addr;
  let rA  = addr{20-16};
  let imm = addr{15-0};
}

// LHUI Rd, Ra, imm — load halfword unsigned  (opcode=0x39)
def LHUI : MBB<0x39, (outs GPR:$rD), (ins memri:$addr),
               "lhui\t$rD, $addr", []> {
  bits<21> addr;
  let rA  = addr{20-16};
  let imm = addr{15-0};
}

// LBUI Rd, Ra, imm — load byte unsigned  (opcode=0x38)
def LBUI : MBB<0x38, (outs GPR:$rD), (ins memri:$addr),
               "lbui\t$rD, $addr", []> {
  bits<21> addr;
  let rA  = addr{20-16};
  let imm = addr{15-0};
}

//===----------------------------------------------------------------------===//
// Load instructions — register offset (Type A)
//===----------------------------------------------------------------------===//

// LW Rd, Ra, Rb — load word  (opcode=0x32, func=0)
def LW   : MBA<0x32, 0, (outs GPR:$rD), (ins memrr:$addr),
               "lw\t$rD, $addr", []> {
  bits<10> addr;   // [9:5]=rA [4:0]=rB
  let rA = addr{9-5};
  let rB = addr{4-0};
}

// LHU Rd, Ra, Rb — load halfword unsigned  (opcode=0x31, func=0)
def LHU  : MBA<0x31, 0, (outs GPR:$rD), (ins memrr:$addr),
               "lhu\t$rD, $addr", []> {
  bits<10> addr;
  let rA = addr{9-5};
  let rB = addr{4-0};
}

// LBU Rd, Ra, Rb — load byte unsigned  (opcode=0x30, func=0)
def LBU  : MBA<0x30, 0, (outs GPR:$rD), (ins memrr:$addr),
               "lbu\t$rD, $addr", []> {
  bits<10> addr;
  let rA = addr{9-5};
  let rB = addr{4-0};
}

} // let mayLoad = 1, hasSideEffects = 0

//===----------------------------------------------------------------------===//
// Byte-reversed load instructions — register offset (Type A, func=0x200)
// UG984 §5. Loads bytes in reversed order (for endian conversion).
// lwr Rd, Ra, Rb: Rd = mem[Ra+Rb] with byte order reversed.
// These instructions have no compiler DAG patterns; used in hand-written
// assembly or inline asm for endian conversion.
//===----------------------------------------------------------------------===//

let mayLoad = 1, hasSideEffects = 0 in {

def LWR  : MBA<0x32, 0x200, (outs GPR:$rD), (ins memrr:$addr),
               "lwr\t$rD, $addr", []> {
  bits<10> addr;
  let rA = addr{9-5};
  let rB = addr{4-0};
}

def LHUR : MBA<0x31, 0x200, (outs GPR:$rD), (ins memrr:$addr),
               "lhur\t$rD, $addr", []> {
  bits<10> addr;
  let rA = addr{9-5};
  let rB = addr{4-0};
}

def LBUR : MBA<0x30, 0x200, (outs GPR:$rD), (ins memrr:$addr),
               "lbur\t$rD, $addr", []> {
  bits<10> addr;
  let rA = addr{9-5};
  let rB = addr{4-0};
}

} // let mayLoad = 1, hasSideEffects = 0

//===----------------------------------------------------------------------===//
// Byte-reversed store instructions — register offset (Type A, func=0x200)
// UG984 §5. Stores bytes in reversed order.
//===----------------------------------------------------------------------===//

let mayStore = 1, hasSideEffects = 0 in {

def SWR  : MBA<0x36, 0x200, (outs), (ins GPR:$rD, memrr:$addr),
               "swr\t$rD, $addr", []> {
  bits<10> addr;
  let rA = addr{9-5};
  let rB = addr{4-0};
}

def SHR  : MBA<0x35, 0x200, (outs), (ins GPR:$rD, memrr:$addr),
               "shr\t$rD, $addr", []> {
  bits<10> addr;
  let rA = addr{9-5};
  let rB = addr{4-0};
}

def SBR  : MBA<0x34, 0x200, (outs), (ins GPR:$rD, memrr:$addr),
               "sbr\t$rD, $addr", []> {
  bits<10> addr;
  let rA = addr{9-5};
  let rB = addr{4-0};
}

} // let mayStore = 1, hasSideEffects = 0

//===----------------------------------------------------------------------===//
// Extended-address (EA) load instructions — register offset (Type A, func=0x080)
// UG984 §5. Addr = Ra:Rb (64-bit concatenated address); requires C_ADDR_SIZE > 32
// (MB-X with +extended-addr). No compiler DAG patterns — assembler/disassembler only.
//===----------------------------------------------------------------------===//

let Predicates = [HasExtendedAddrPred], mayLoad = 1, hasSideEffects = 0 in {

def LWEA  : MBA<0x32, 0x080, (outs GPR:$rD), (ins memrr:$addr),
                "lwea\t$rD, $addr", []> {
  bits<10> addr;
  let rA = addr{9-5};
  let rB = addr{4-0};
}

def LHUEA : MBA<0x31, 0x080, (outs GPR:$rD), (ins memrr:$addr),
                "lhuea\t$rD, $addr", []> {
  bits<10> addr;
  let rA = addr{9-5};
  let rB = addr{4-0};
}

def LBUEA : MBA<0x30, 0x080, (outs GPR:$rD), (ins memrr:$addr),
                "lbuea\t$rD, $addr", []> {
  bits<10> addr;
  let rA = addr{9-5};
  let rB = addr{4-0};
}

} // Predicates = [HasExtendedAddrPred], mayLoad

//===----------------------------------------------------------------------===//
// Extended-address (EA) store instructions — register offset (Type A, func=0x080)
//===----------------------------------------------------------------------===//

let Predicates = [HasExtendedAddrPred], mayStore = 1, hasSideEffects = 0 in {

def SWEA  : MBA<0x36, 0x080, (outs), (ins GPR:$rD, memrr:$addr),
                "swea\t$rD, $addr", []> {
  bits<10> addr;
  let rA = addr{9-5};
  let rB = addr{4-0};
}

def SHEA  : MBA<0x35, 0x080, (outs), (ins GPR:$rD, memrr:$addr),
                "shea\t$rD, $addr", []> {
  bits<10> addr;
  let rA = addr{9-5};
  let rB = addr{4-0};
}

def SBEA  : MBA<0x34, 0x080, (outs), (ins GPR:$rD, memrr:$addr),
                "sbea\t$rD, $addr", []> {
  bits<10> addr;
  let rA = addr{9-5};
  let rB = addr{4-0};
}

} // Predicates = [HasExtendedAddrPred], mayStore

//===----------------------------------------------------------------------===//
// Store instructions — immediate offset (Type B)
//===----------------------------------------------------------------------===//

let mayStore = 1, hasSideEffects = 0 in {

// SWI Rd, Ra, imm — store word  (opcode=0x3E)
def SWI : MBB<0x3E, (outs), (ins GPR:$rD, memri:$addr),
              "swi\t$rD, $addr", []> {
  bits<21> addr;
  let rA  = addr{20-16};
  let imm = addr{15-0};
}

// SHI Rd, Ra, imm — store halfword  (opcode=0x3D)
def SHI : MBB<0x3D, (outs), (ins GPR:$rD, memri:$addr),
              "shi\t$rD, $addr", []> {
  bits<21> addr;
  let rA  = addr{20-16};
  let imm = addr{15-0};
}

// SBI Rd, Ra, imm — store byte  (opcode=0x3C)
def SBI : MBB<0x3C, (outs), (ins GPR:$rD, memri:$addr),
              "sbi\t$rD, $addr", []> {
  bits<21> addr;
  let rA  = addr{20-16};
  let imm = addr{15-0};
}

//===----------------------------------------------------------------------===//
// Store instructions — register offset (Type A)
//===----------------------------------------------------------------------===//

// SW Rd, Ra, Rb — store word  (opcode=0x36, func=0)
def SW : MBA<0x36, 0, (outs), (ins GPR:$rD, memrr:$addr),
             "sw\t$rD, $addr", []> {
  bits<10> addr;
  let rA = addr{9-5};
  let rB = addr{4-0};
}

// SH Rd, Ra, Rb — store halfword  (opcode=0x35, func=0)
def SH : MBA<0x35, 0, (outs), (ins GPR:$rD, memrr:$addr),
             "sh\t$rD, $addr", []> {
  bits<10> addr;
  let rA = addr{9-5};
  let rB = addr{4-0};
}

// SB Rd, Ra, Rb — store byte  (opcode=0x34, func=0)
def SB : MBA<0x34, 0, (outs), (ins GPR:$rD, memrr:$addr),
             "sb\t$rD, $addr", []> {
  bits<10> addr;
  let rA = addr{9-5};
  let rB = addr{4-0};
}

} // let mayStore = 1, hasSideEffects = 0

//===----------------------------------------------------------------------===//
// Memory SelectionDAG patterns
//===----------------------------------------------------------------------===//

// Load word: base + reg-offset or base + imm16-offset.
def : Pat<(i32 (load ADDRri:$addr)), (LWI ADDRri:$addr)>;
def : Pat<(i32 (load ADDRrr:$addr)), (LW  ADDRrr:$addr)>;

// Load half-word (zero-extend and anyext — LHUI always zero-extends).
def : Pat<(i32 (zextloadi16 ADDRri:$addr)), (LHUI ADDRri:$addr)>;
def : Pat<(i32 (zextloadi16 ADDRrr:$addr)), (LHU  ADDRrr:$addr)>;
def : Pat<(i32 (extloadi16  ADDRri:$addr)), (LHUI ADDRri:$addr)>;
def : Pat<(i32 (extloadi16  ADDRrr:$addr)), (LHU  ADDRrr:$addr)>;

// Load byte (zero-extend and anyext — LBUI always zero-extends).
def : Pat<(i32 (zextloadi8 ADDRri:$addr)), (LBUI ADDRri:$addr)>;
def : Pat<(i32 (zextloadi8 ADDRrr:$addr)), (LBU  ADDRrr:$addr)>;
def : Pat<(i32 (extloadi8  ADDRri:$addr)), (LBUI ADDRri:$addr)>;
def : Pat<(i32 (extloadi8  ADDRrr:$addr)), (LBU  ADDRrr:$addr)>;

// Store word.
def : Pat<(store i32:$rd, ADDRri:$addr), (SWI GPR:$rd, ADDRri:$addr)>;
def : Pat<(store i32:$rd, ADDRrr:$addr), (SW  GPR:$rd, ADDRrr:$addr)>;

// Store half-word.
def : Pat<(truncstorei16 i32:$rd, ADDRri:$addr), (SHI GPR:$rd, ADDRri:$addr)>;
def : Pat<(truncstorei16 i32:$rd, ADDRrr:$addr), (SH  GPR:$rd, ADDRrr:$addr)>;

// Store byte.
def : Pat<(truncstorei8 i32:$rd, ADDRri:$addr), (SBI GPR:$rd, ADDRri:$addr)>;
def : Pat<(truncstorei8 i32:$rd, ADDRrr:$addr), (SB  GPR:$rd, ADDRrr:$addr)>;
