//===- IntrinsicsMicroBlaze.td - Defines MicroBlaze intrinsics --*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines all of the MicroBlaze-specific intrinsics.
//
// FSL (Fast Simplex Link) / AXI-Stream access (UG984 §5).  Each FSL get/put
// flag combination has one intrinsic; the backend selects the static encoding
// (port in the instruction word as rfslN) when the port is a constant in 0..15,
// and the dynamic encoding (port in a GPR) otherwise.  See MicroBlazeInstrFSL.td.
//
//   i32  @llvm.microblaze.fsl.<flags>get(i32 port)
//   void @llvm.microblaze.fsl.<flags>put(i32 value, i32 port)
//
// Each maps to a Clang builtin __builtin_microblaze_fsl_<flags>{get,put} so the
// channels are reachable directly from C; the generic builtin→intrinsic path in
// clang lowers them automatically (the arch prefix "microblaze" matches the
// TargetPrefix below).
//
// The intrinsics have unmodelled side effects (FIFO reads/writes must not be
// reordered, duplicated, or eliminated), so they are left with default memory
// properties (i.e. not marked IntrNoMem).
//
//===----------------------------------------------------------------------===//

let TargetPrefix = "microblaze" in {

  // GET: read one word from an FSL channel into a GPR.
  foreach name = ["get",   "eget",   "aget",   "eaget",
                  "tget",  "teget",  "taget",  "teaget",
                  "cget",  "ecget",  "caget",  "ecaget",
                  "tcget", "tecget", "tcaget", "tecaget",
                  "nget",  "neget",  "naget",  "neaget",
                  "tnget", "tneget", "tnaget", "tneaget",
                  "ncget", "necget", "ncaget", "necaget",
                  "tncget","tnecget","tncaget","tnecaget"] in
    def int_microblaze_fsl_#name
        : ClangBuiltin<"__builtin_microblaze_fsl_" # name>,
          Intrinsic<[llvm_i32_ty], [llvm_i32_ty]>;

  // PUT: write one GPR word to an FSL channel.
  foreach name = ["put",   "eput",   "aput",   "eaput",
                  "tput",  "teput",  "taput",  "teaput",
                  "cput",  "ecput",  "caput",  "ecaput",
                  "tcput", "tecput", "tcaput", "tecaput",
                  "nput",  "neput",  "naput",  "neaput",
                  "tnput", "tneput", "tnaput", "tneaput",
                  "ncput", "necput", "ncaput", "necaput",
                  "tncput","tnecput","tncaput","tnecaput"] in
    def int_microblaze_fsl_#name
        : ClangBuiltin<"__builtin_microblaze_fsl_" # name>,
          Intrinsic<[], [llvm_i32_ty, llvm_i32_ty]>;

} // TargetPrefix = "microblaze"
