//===-- MicroBlazeInstrMem.td - MicroBlaze Instruction Definitions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// Load instructions — immediate offset (Type B)
//===----------------------------------------------------------------------===//

// Load instructions use a combined memri/memrr address operand so that the
// ADDRri/ADDRrr ComplexPatterns can match them directly.
// The 21-bit addr field encodes [20:16]=base register and [15:0]=imm offset.

// LWI  Rd, Ra, imm — load word     (opcode=0x3A)
def LWI  : MBB<0x3A, (outs GPR:$rD), (ins memri:$addr),
               "lwi\t$rD, $addr", [], IIC_LD> {
  bits<21> addr;
  let rA  = addr{20-16};
  let imm = addr{15-0};
}

// LHUI Rd, Ra, imm — load halfword unsigned  (opcode=0x39)
def LHUI : MBB<0x39, (outs GPR:$rD), (ins memri:$addr),
               "lhui\t$rD, $addr", [], IIC_LD> {
  bits<21> addr;
  let rA  = addr{20-16};
  let imm = addr{15-0};
}

// LBUI Rd, Ra, imm — load byte unsigned  (opcode=0x38)
def LBUI : MBB<0x38, (outs GPR:$rD), (ins memri:$addr),
               "lbui\t$rD, $addr", [], IIC_LD> {
  bits<21> addr;
  let rA  = addr{20-16};
  let imm = addr{15-0};
}

//===----------------------------------------------------------------------===//
// Load instructions — register offset (Type A)
//===----------------------------------------------------------------------===//

// LW Rd, Ra, Rb — load word  (opcode=0x32, func=0)
def LW   : MBA<0x32, 0, (outs GPR:$rD), (ins memrr:$addr),
               "lw\t$rD, $addr", [], IIC_LD> {
  bits<10> addr;   // [9:5]=rA [4:0]=rB
  let rA = addr{9-5};
  let rB = addr{4-0};
}

// LHU Rd, Ra, Rb — load halfword unsigned  (opcode=0x31, func=0)
def LHU  : MBA<0x31, 0, (outs GPR:$rD), (ins memrr:$addr),
               "lhu\t$rD, $addr", [], IIC_LD> {
  bits<10> addr;
  let rA = addr{9-5};
  let rB = addr{4-0};
}

// LBU Rd, Ra, Rb — load byte unsigned  (opcode=0x30, func=0)
def LBU  : MBA<0x30, 0, (outs GPR:$rD), (ins memrr:$addr),
               "lbu\t$rD, $addr", [], IIC_LD> {
  bits<10> addr;
  let rA = addr{9-5};
  let rB = addr{4-0};
}

//===----------------------------------------------------------------------===//
// Store instructions — immediate offset (Type B)
//===----------------------------------------------------------------------===//

// SWI Rd, Ra, imm — store word  (opcode=0x3E)
def SWI : MBB<0x3E, (outs), (ins GPR:$rD, memri:$addr),
              "swi\t$rD, $addr", [], IIC_ST> {
  bits<21> addr;
  let rA  = addr{20-16};
  let imm = addr{15-0};
}

// SHI Rd, Ra, imm — store halfword  (opcode=0x3D)
def SHI : MBB<0x3D, (outs), (ins GPR:$rD, memri:$addr),
              "shi\t$rD, $addr", [], IIC_ST> {
  bits<21> addr;
  let rA  = addr{20-16};
  let imm = addr{15-0};
}

// SBI Rd, Ra, imm — store byte  (opcode=0x3C)
def SBI : MBB<0x3C, (outs), (ins GPR:$rD, memri:$addr),
              "sbi\t$rD, $addr", [], IIC_ST> {
  bits<21> addr;
  let rA  = addr{20-16};
  let imm = addr{15-0};
}

//===----------------------------------------------------------------------===//
// Store instructions — register offset (Type A)
//===----------------------------------------------------------------------===//

// SW Rd, Ra, Rb — store word  (opcode=0x36, func=0)
def SW : MBA<0x36, 0, (outs), (ins GPR:$rD, memrr:$addr),
             "sw\t$rD, $addr", [], IIC_ST> {
  bits<10> addr;
  let rA = addr{9-5};
  let rB = addr{4-0};
}

// SH Rd, Ra, Rb — store halfword  (opcode=0x35, func=0)
def SH : MBA<0x35, 0, (outs), (ins GPR:$rD, memrr:$addr),
             "sh\t$rD, $addr", [], IIC_ST> {
  bits<10> addr;
  let rA = addr{9-5};
  let rB = addr{4-0};
}

// SB Rd, Ra, Rb — store byte  (opcode=0x34, func=0)
def SB : MBA<0x34, 0, (outs), (ins GPR:$rD, memrr:$addr),
             "sb\t$rD, $addr", [], IIC_ST> {
  bits<10> addr;
  let rA = addr{9-5};
  let rB = addr{4-0};
}

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
