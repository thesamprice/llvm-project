//===-- MicroBlazeCallingConv.td - MicroBlaze Calling Convention -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// MicroBlaze ILP32 calling convention (little-endian, microblazeel).
//
// Argument registers: R5–R10 (6 × i32).  Additional arguments are passed on
// the stack at 4-byte alignment.  Structures are passed by value: the
// MicroBlazeABIInfo in clang/lib/CodeGen/Targets/MicroBlaze.cpp coerces
// aggregates to [N x i32] before the call, so only i32 scalars reach here.
//
// Return registers: R3 for a single i32/f32 word; R3:R4 for an i64/f64 pair
// (R3 = low word, R4 = high word).
//
// Callee-saved registers: R22–R31.  The callee-saved set is also recorded in
// MicroBlazeRegisterInfo.td as CSR so that getCalleeSavedRegs() and
// getCallPreservedMask() agree with these definitions.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Return-value convention
//===----------------------------------------------------------------------===//

def RetCC_MicroBlaze : CallingConv<[
  // i32 / f32: first word in R3, second word (64-bit pair) in R4.
  CCIfType<[i32, f32], CCAssignToReg<[R3, R4]>>,

  // Anything that does not fit in registers spills to the stack.
  CCAssignToStack<4, 4>
]>;

//===----------------------------------------------------------------------===//
// Argument-passing convention
//===----------------------------------------------------------------------===//

def CC_MicroBlaze : CallingConv<[
  // i32 / f32 scalar arguments: fill R5–R10 in order.
  // Arguments beyond the sixth spill to the caller's outgoing stack area.
  CCIfType<[i32, f32], CCAssignToReg<[R5, R6, R7, R8, R9, R10]>>,

  // Stack overflow: 4-byte slot, 4-byte aligned.
  CCAssignToStack<4, 4>
]>;
