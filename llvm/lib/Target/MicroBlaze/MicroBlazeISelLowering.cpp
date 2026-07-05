//===-- MicroBlazeISelLowering.cpp - MicroBlaze DAG Lowering --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MicroBlazeISelLowering.h"
#include "MCTargetDesc/MicroBlazeMCTargetDesc.h"
#include "MicroBlaze.h"
#include "MicroBlazeBaseInfo.h"
#include "MicroBlazeInstrInfo.h"
#include "MicroBlazeMachineFunctionInfo.h"
#include "MicroBlazeSubtarget.h"
#include "MicroBlazeTargetMachine.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/RuntimeLibcallUtil.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/LLVMContext.h"

using namespace llvm;

// Must follow "using namespace llvm" — generated code uses unqualified names.
#include "MicroBlazeGenCallingConv.inc"

//===----------------------------------------------------------------------===//
// Constructor / configuration
//===----------------------------------------------------------------------===//

MicroBlazeTargetLowering::MicroBlazeTargetLowering(
    const MicroBlazeTargetMachine &TM, const MicroBlazeSubtarget &STI)
    : TargetLowering(TM, STI), Subtarget(STI) {
  // i32 is always a native value type.
  addRegisterClass(MVT::i32, &MicroBlaze::GPRRegClass);

  // With +hard-float, f32 values live in the same GPRs and are operated on
  // by hardware FPU instructions (opcode 0x16).  Register f32 as a legal
  // type so SelectionDAG can type-check and allocate FPR operands.
  if (STI.hasHardFloat())
    addRegisterClass(MVT::f32, &MicroBlaze::FPRRegClass);

  computeRegisterProperties(STI.getRegisterInfo());

  setStackPointerRegisterToSaveRestore(MicroBlaze::R1);
  setBooleanContents(ZeroOrOneBooleanContent);

  // Atomic operations: LWX/SWX provide 32-bit hardware LL/SC.
  // AtomicExpandPass generates the retry loop using emitLoadLinked /
  // emitStoreConditional (defined below); setMinCmpXchgSizeInBits(32) promotes
  // sub-word cmpxchg to i32 width with masking.  ATOMIC_FENCE is Custom
  // (→ MBAR) and is unaffected by these settings.
  setMaxAtomicSizeInBitsSupported(32);
  setMinCmpXchgSizeInBits(32);

  // Carry-chain operations for i64 arithmetic.  MSR_C is modeled as a
  // physical carry register (like ARM's CPSR) so the carry flows directly
  // between ADD/ADDC and RSUB/RSUBC without any GPR round-trip.
  // UADDO/USUBO/UADDO_CARRY/USUBO_CARRY are Custom-lowered to
  // MicroBlazeISD::ADDC/ADDE/SUBC/SUBE nodes that use MSR_C explicitly.
  // The DAGCombine in PerformDAGCombine eliminates the boolean↔flags
  // round-trip so the final output is always 2 instructions per 32-bit word.
  setOperationAction(ISD::UADDO, MVT::i32, Custom);
  setOperationAction(ISD::USUBO, MVT::i32, Custom);
  setOperationAction(ISD::UADDO_CARRY, MVT::i32, Custom);
  setOperationAction(ISD::USUBO_CARRY, MVT::i32, Custom);

  // Integer divide: use hardware IDIV/IDIVU when +divide is active; otherwise
  // fall through to __divsi3/__udivsi3 libcalls.  SREM/UREM always use
  // libcalls because the remainder is a destructive rA side-effect of IDIV
  // that is not currently modeled as a separate DAG output.
  if (!STI.hasDivide()) {
    setOperationAction(ISD::SDIV, MVT::i32, Expand);
    setOperationAction(ISD::UDIV, MVT::i32, Expand);
  }
  setOperationAction(ISD::SREM, MVT::i32, Expand);
  setOperationAction(ISD::UREM, MVT::i32, Expand);
  setOperationAction(ISD::SDIVREM, MVT::i32, Expand);
  setOperationAction(ISD::UDIVREM, MVT::i32, Expand);

  // MicroBlaze is not in LLVM's LegacyDefaultSystemLibrary predicate, so the
  // RuntimeLibcallsInfo table leaves all software-ABI routines as Unsupported.
  // Register them explicitly so that Expand actions emit actual calls.
  setLibcallImpl(RTLIB::SDIV_I32, RTLIB::impl___divsi3);
  setLibcallImpl(RTLIB::UDIV_I32, RTLIB::impl___udivsi3);
  setLibcallImpl(RTLIB::SREM_I32, RTLIB::impl___modsi3);
  setLibcallImpl(RTLIB::UREM_I32, RTLIB::impl___umodsi3);
  setLibcallImpl(RTLIB::MUL_I32, RTLIB::impl___mulsi3);

  // Memory intrinsics: inline small copies as load/store sequences.
  // The default threshold (4 stores = 16 bytes) is too low for idiomatic C
  // struct copies (e.g. dhrystone's 48-byte Rec_Type).  Raise to 16 stores
  // (64 bytes) so LLVM inlines these the same way GCC does at -O2.
  // Copies larger than 64 bytes still fall through to a memcpy libcall.
  MaxStoresPerMemcpy = 16;
  MaxStoresPerMemcpyOptSize = 8;
  MaxStoresPerMemset = 16;
  MaxStoresPerMemsetOptSize = 8;
  MaxStoresPerMemmove = 16;
  MaxStoresPerMemmoveOptSize = 8;
  setLibcallImpl(RTLIB::MEMCPY, RTLIB::impl_memcpy);
  setLibcallImpl(RTLIB::MEMMOVE, RTLIB::impl_memmove);
  setLibcallImpl(RTLIB::MEMSET, RTLIB::impl_memset);

  // 64-bit arithmetic: LLVM expands i64 ops on this 32-bit target via libcalls.
  setLibcallImpl(RTLIB::SDIV_I64, RTLIB::impl___divdi3);
  setLibcallImpl(RTLIB::UDIV_I64, RTLIB::impl___udivdi3);
  setLibcallImpl(RTLIB::SREM_I64, RTLIB::impl___moddi3);
  setLibcallImpl(RTLIB::UREM_I64, RTLIB::impl___umoddi3);
  setLibcallImpl(RTLIB::MUL_I64, RTLIB::impl___muldi3);
  setLibcallImpl(RTLIB::SHL_I64, RTLIB::impl___ashldi3);
  setLibcallImpl(RTLIB::SRL_I64, RTLIB::impl___lshrdi3);
  setLibcallImpl(RTLIB::SRA_I64, RTLIB::impl___ashrdi3);

  // Shifts: legal with barrel-shift (TableGen patterns handle both register and
  // immediate forms). Without barrel-shift, lower to compiler-rt libcalls
  // (__lshlsi3 / __lshrsi3 / __ashrsi3) via Custom lowering; ISD::Expand is
  // intentionally avoided because its ExpandNode path mishandles scalar shifts
  // in release builds (asserts VT.isVector() which is disabled in release).
  if (!STI.hasBarrelShift()) {
    setLibcallImpl(RTLIB::SHL_I32, RTLIB::impl___ashlsi3);
    setLibcallImpl(RTLIB::SRL_I32, RTLIB::impl___lshrsi3);
    setLibcallImpl(RTLIB::SRA_I32, RTLIB::impl___ashrsi3);

    setOperationAction(ISD::SHL, MVT::i32, Custom);
    setOperationAction(ISD::SRL, MVT::i32, Custom);
    setOperationAction(ISD::SRA, MVT::i32, Custom);
  }

  // MUL is always available; high-word variants need +multiply-high.
  // With +multiply-high, mark MULHS/MULHU Legal so the DAGCombiner's
  // divide-by-constant strength reduction can emit multiply-by-magic sequences
  // (idiv is 34 cycles; mulh is 3 cycles — always prefer magic for constants).
  // Without +multiply-high, Expand all high-multiply ops so constant division
  // falls through to __divsi3/__modsi3 libcalls.
  //
  // SMUL_LOHI/UMUL_LOHI must always be Expand: when +multiply-high is active
  // the SelectionDAGLegalize expander splits them into MUL (low) + MULHS/MULHU
  // (high); without it they fall to libcalls.  Without this unconditional
  // Expand the ops default to Legal and ISel crashes with "Cannot select".
  setOperationAction(ISD::SMUL_LOHI, MVT::i32, Expand);
  setOperationAction(ISD::UMUL_LOHI, MVT::i32, Expand);
  if (STI.hasMultiplyHigh()) {
    setOperationAction(ISD::MULHS, MVT::i32, Legal);
    setOperationAction(ISD::MULHU, MVT::i32, Legal);
  } else {
    setOperationAction(ISD::MULHS, MVT::i32, Expand);
    setOperationAction(ISD::MULHU, MVT::i32, Expand);
  }

  // _Bool is stored as a byte; promote i1 ext-loads to i8 so the existing
  // zextloadi8/extloadi8 patterns can select them.
  setLoadExtAction(ISD::ZEXTLOAD, MVT::i32, MVT::i1, Promote);
  setLoadExtAction(ISD::EXTLOAD, MVT::i32, MVT::i1, Promote);
  setLoadExtAction(ISD::SEXTLOAD, MVT::i32, MVT::i1, Expand);

  // No sign-extending loads (only zero-extend for bytes/halves).
  // Expand to zero-extending load + sext8/sext16.
  setLoadExtAction(ISD::SEXTLOAD, MVT::i32, MVT::i8, Expand);
  setLoadExtAction(ISD::SEXTLOAD, MVT::i32, MVT::i16, Expand);

  // sext8/sext16 are base-ISA instructions (UG984 Figs 129-130).
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1, Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i8, Legal);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i16, Legal);

  // Without hard-float, all floating-point operations are done via libcalls.
  // Expand both f32 and f64 operations; the legalizer emits calls to the
  // compiler-rt soft-float routines registered below.
  if (!STI.hasHardFloat()) {
    for (MVT FT : {MVT::f32, MVT::f64}) {
      setOperationAction(ISD::FADD, FT, Expand);
      setOperationAction(ISD::FSUB, FT, Expand);
      setOperationAction(ISD::FMUL, FT, Expand);
      setOperationAction(ISD::FDIV, FT, Expand);
      setOperationAction(ISD::FREM, FT, Expand);
      setOperationAction(ISD::FMA, FT, Expand);
      setOperationAction(ISD::FNEG, FT, Expand);
      setOperationAction(ISD::FABS, FT, Expand);
      setOperationAction(ISD::FSQRT, FT, Expand);
      setOperationAction(ISD::FSIN, FT, Expand);
      setOperationAction(ISD::FCOS, FT, Expand);
      setOperationAction(ISD::FPOW, FT, Expand);
      setOperationAction(ISD::FLOG, FT, Expand);
      setOperationAction(ISD::FLOG2, FT, Expand);
      setOperationAction(ISD::FLOG10, FT, Expand);
      setOperationAction(ISD::FEXP, FT, Expand);
      setOperationAction(ISD::FEXP2, FT, Expand);
      setOperationAction(ISD::FRINT, FT, Expand);
      setOperationAction(ISD::FNEARBYINT, FT, Expand);
      setOperationAction(ISD::FCEIL, FT, Expand);
      setOperationAction(ISD::FFLOOR, FT, Expand);
      setOperationAction(ISD::FTRUNC, FT, Expand);
      setOperationAction(ISD::FROUND, FT, Expand);
      setOperationAction(ISD::FMINNUM, FT, Expand);
      setOperationAction(ISD::FMAXNUM, FT, Expand);
      setOperationAction(ISD::FCOPYSIGN, FT, Expand);
      setOperationAction(ISD::FLDEXP, FT, Expand);
    }
    setOperationAction(ISD::FP_TO_SINT, MVT::i32, Expand);
    setOperationAction(ISD::FP_TO_SINT, MVT::i64, Expand);
    setOperationAction(ISD::FP_TO_UINT, MVT::i32, Expand);
    setOperationAction(ISD::FP_TO_UINT, MVT::i64, Expand);
    setOperationAction(ISD::SINT_TO_FP, MVT::i32, Expand);
    setOperationAction(ISD::SINT_TO_FP, MVT::i64, Expand);
    setOperationAction(ISD::UINT_TO_FP, MVT::i32, Expand);
    setOperationAction(ISD::UINT_TO_FP, MVT::i64, Expand);
    setOperationAction(ISD::FP_EXTEND, MVT::f64, Expand);
    setOperationAction(ISD::FP_ROUND, MVT::f32, Expand);

    // Standard compiler-rt soft-float ABI (same names as GCC libgcc).
    // f32 arithmetic
    setLibcallImpl(RTLIB::ADD_F32, RTLIB::impl___addsf3);
    setLibcallImpl(RTLIB::SUB_F32, RTLIB::impl___subsf3);
    setLibcallImpl(RTLIB::MUL_F32, RTLIB::impl___mulsf3);
    setLibcallImpl(RTLIB::DIV_F32, RTLIB::impl___divsf3);
    // f64 arithmetic
    setLibcallImpl(RTLIB::ADD_F64, RTLIB::impl___adddf3);
    setLibcallImpl(RTLIB::SUB_F64, RTLIB::impl___subdf3);
    setLibcallImpl(RTLIB::MUL_F64, RTLIB::impl___muldf3);
    setLibcallImpl(RTLIB::DIV_F64, RTLIB::impl___divdf3);
    // f32 ↔ f64
    setLibcallImpl(RTLIB::FPEXT_F32_F64, RTLIB::impl___extendsfdf2);
    setLibcallImpl(RTLIB::FPROUND_F64_F32, RTLIB::impl___truncdfsf2);
    // f32 → integer
    setLibcallImpl(RTLIB::FPTOSINT_F32_I32, RTLIB::impl___fixsfsi);
    setLibcallImpl(RTLIB::FPTOSINT_F32_I64, RTLIB::impl___fixsfdi);
    setLibcallImpl(RTLIB::FPTOUINT_F32_I32, RTLIB::impl___fixunssfsi);
    setLibcallImpl(RTLIB::FPTOUINT_F32_I64, RTLIB::impl___fixunssfdi);
    // f64 → integer
    setLibcallImpl(RTLIB::FPTOSINT_F64_I32, RTLIB::impl___fixdfsi);
    setLibcallImpl(RTLIB::FPTOSINT_F64_I64, RTLIB::impl___fixdfdi);
    setLibcallImpl(RTLIB::FPTOUINT_F64_I32, RTLIB::impl___fixunsdfsi);
    setLibcallImpl(RTLIB::FPTOUINT_F64_I64, RTLIB::impl___fixunsdfdi);
    // integer → f32
    setLibcallImpl(RTLIB::SINTTOFP_I32_F32, RTLIB::impl___floatsisf);
    setLibcallImpl(RTLIB::SINTTOFP_I64_F32, RTLIB::impl___floatdisf);
    setLibcallImpl(RTLIB::UINTTOFP_I32_F32, RTLIB::impl___floatunsisf);
    setLibcallImpl(RTLIB::UINTTOFP_I64_F32, RTLIB::impl___floatundisf);
    // integer → f64
    setLibcallImpl(RTLIB::SINTTOFP_I32_F64, RTLIB::impl___floatsidf);
    setLibcallImpl(RTLIB::SINTTOFP_I64_F64, RTLIB::impl___floatdidf);
    setLibcallImpl(RTLIB::UINTTOFP_I32_F64, RTLIB::impl___floatunsidf);
    setLibcallImpl(RTLIB::UINTTOFP_I64_F64, RTLIB::impl___floatundidf);
    // f32 comparisons
    setLibcallImpl(RTLIB::OEQ_F32, RTLIB::impl___eqsf2);
    setLibcallImpl(RTLIB::UNE_F32, RTLIB::impl___nesf2);
    setLibcallImpl(RTLIB::OLT_F32, RTLIB::impl___ltsf2);
    setLibcallImpl(RTLIB::OLE_F32, RTLIB::impl___lesf2);
    setLibcallImpl(RTLIB::OGT_F32, RTLIB::impl___gtsf2);
    setLibcallImpl(RTLIB::OGE_F32, RTLIB::impl___gesf2);
    setLibcallImpl(RTLIB::UO_F32, RTLIB::impl___unordsf2);
    // f64 comparisons
    setLibcallImpl(RTLIB::OEQ_F64, RTLIB::impl___eqdf2);
    setLibcallImpl(RTLIB::UNE_F64, RTLIB::impl___nedf2);
    setLibcallImpl(RTLIB::OLT_F64, RTLIB::impl___ltdf2);
    setLibcallImpl(RTLIB::OLE_F64, RTLIB::impl___ledf2);
    setLibcallImpl(RTLIB::OGT_F64, RTLIB::impl___gtdf2);
    setLibcallImpl(RTLIB::OGE_F64, RTLIB::impl___gedf2);
    setLibcallImpl(RTLIB::UO_F64, RTLIB::impl___unorddf2);
    // f32 math libcalls (C99 single-precision variants)
    setLibcallImpl(RTLIB::FLOOR_F32, RTLIB::impl_floorf);
    setLibcallImpl(RTLIB::CEIL_F32, RTLIB::impl_ceilf);
    setLibcallImpl(RTLIB::ROUND_F32, RTLIB::impl_roundf);
    setLibcallImpl(RTLIB::TRUNC_F32, RTLIB::impl_truncf);
    setLibcallImpl(RTLIB::RINT_F32, RTLIB::impl_rintf);
    setLibcallImpl(RTLIB::NEARBYINT_F32, RTLIB::impl_nearbyintf);
    setLibcallImpl(RTLIB::SQRT_F32, RTLIB::impl_sqrtf);
    setLibcallImpl(RTLIB::SIN_F32, RTLIB::impl_sinf);
    setLibcallImpl(RTLIB::COS_F32, RTLIB::impl_cosf);
    setLibcallImpl(RTLIB::POW_F32, RTLIB::impl_powf);
    setLibcallImpl(RTLIB::EXP_F32, RTLIB::impl_expf);
    setLibcallImpl(RTLIB::EXP2_F32, RTLIB::impl_exp2f);
    setLibcallImpl(RTLIB::LOG_F32, RTLIB::impl_logf);
    setLibcallImpl(RTLIB::LOG2_F32, RTLIB::impl_log2f);
    setLibcallImpl(RTLIB::LOG10_F32, RTLIB::impl_log10f);
    setLibcallImpl(RTLIB::REM_F32, RTLIB::impl_fmodf);
    setLibcallImpl(RTLIB::COPYSIGN_F32, RTLIB::impl_copysignf);
    setLibcallImpl(RTLIB::FMIN_F32, RTLIB::impl_fminf);
    setLibcallImpl(RTLIB::FMAX_F32, RTLIB::impl_fmaxf);
    setLibcallImpl(RTLIB::LDEXP_F32, RTLIB::impl_ldexpf);
    // f64 math libcalls (C99 double-precision)
    setLibcallImpl(RTLIB::FLOOR_F64, RTLIB::impl_floor);
    setLibcallImpl(RTLIB::CEIL_F64, RTLIB::impl_ceil);
    setLibcallImpl(RTLIB::ROUND_F64, RTLIB::impl_round);
    setLibcallImpl(RTLIB::TRUNC_F64, RTLIB::impl_trunc);
    setLibcallImpl(RTLIB::RINT_F64, RTLIB::impl_rint);
    setLibcallImpl(RTLIB::NEARBYINT_F64, RTLIB::impl_nearbyint);
    setLibcallImpl(RTLIB::SQRT_F64, RTLIB::impl_sqrt);
    setLibcallImpl(RTLIB::SIN_F64, RTLIB::impl_sin);
    setLibcallImpl(RTLIB::COS_F64, RTLIB::impl_cos);
    setLibcallImpl(RTLIB::POW_F64, RTLIB::impl_pow);
    setLibcallImpl(RTLIB::EXP_F64, RTLIB::impl_exp);
    setLibcallImpl(RTLIB::EXP2_F64, RTLIB::impl_exp2);
    setLibcallImpl(RTLIB::LOG_F64, RTLIB::impl_log);
    setLibcallImpl(RTLIB::LOG2_F64, RTLIB::impl_log2);
    setLibcallImpl(RTLIB::LOG10_F64, RTLIB::impl_log10);
    setLibcallImpl(RTLIB::REM_F64, RTLIB::impl_fmod);
    setLibcallImpl(RTLIB::COPYSIGN_F64, RTLIB::impl_copysign);
    setLibcallImpl(RTLIB::FMIN_F64, RTLIB::impl_fmin);
    setLibcallImpl(RTLIB::FMAX_F64, RTLIB::impl_fmax);
    setLibcallImpl(RTLIB::LDEXP_F64, RTLIB::impl_ldexp);
  } else {
    // Hard-float: f32 arithmetic is Legal via TableGen FPU instruction
    // patterns.  Operations not covered by hardware remain expanded.
    //
    // FADD / FSUB / FMUL / FDIV are available with +hard-float.
    // FSQRT / SINT_TO_FP / FP_TO_SINT require +float-convert in addition.
    // Everything else (FNEG, FABS, FP extensions, comparisons via SETCC)
    // expands to the soft-float libcall sequence because MicroBlaze provides
    // no dedicated instructions for those operations.
    setOperationAction(ISD::FADD, MVT::f32, Legal);
    setOperationAction(ISD::FSUB, MVT::f32, Legal);
    setOperationAction(ISD::FMUL, MVT::f32, Legal);
    setOperationAction(ISD::FDIV, MVT::f32, Legal);
    if (STI.hasFloatConvert()) {
      setOperationAction(ISD::FSQRT, MVT::f32, Legal);
      setOperationAction(ISD::SINT_TO_FP, MVT::i32, Legal);
      setOperationAction(ISD::FP_TO_SINT, MVT::f32, Legal);
    } else {
      // No hardware float↔int conversion; fall back to __fixsfsi / __floatsisf.
      setOperationAction(ISD::FP_TO_SINT, MVT::i32, Expand);
      setOperationAction(ISD::FP_TO_SINT, MVT::i64, Expand);
      setOperationAction(ISD::FP_TO_UINT, MVT::i32, Expand);
      setOperationAction(ISD::FP_TO_UINT, MVT::i64, Expand);
      setOperationAction(ISD::SINT_TO_FP, MVT::i32, Expand);
      setOperationAction(ISD::SINT_TO_FP, MVT::i64, Expand);
      setOperationAction(ISD::UINT_TO_FP, MVT::i32, Expand);
      setOperationAction(ISD::UINT_TO_FP, MVT::i64, Expand);
      setLibcallImpl(RTLIB::FPTOSINT_F32_I32, RTLIB::impl___fixsfsi);
      setLibcallImpl(RTLIB::FPTOSINT_F32_I64, RTLIB::impl___fixsfdi);
      setLibcallImpl(RTLIB::FPTOUINT_F32_I32, RTLIB::impl___fixunssfsi);
      setLibcallImpl(RTLIB::FPTOUINT_F32_I64, RTLIB::impl___fixunssfdi);
      setLibcallImpl(RTLIB::SINTTOFP_I32_F32, RTLIB::impl___floatsisf);
      setLibcallImpl(RTLIB::SINTTOFP_I64_F32, RTLIB::impl___floatdisf);
      setLibcallImpl(RTLIB::UINTTOFP_I32_F32, RTLIB::impl___floatunsisf);
      setLibcallImpl(RTLIB::UINTTOFP_I64_F32, RTLIB::impl___floatundisf);
    }
    // FNEG has no hardware instruction; expand to (frsub 0.0, x) or libcall.
    setOperationAction(ISD::FNEG, MVT::f32, Expand);
    setOperationAction(ISD::FABS, MVT::f32, Expand);
    setOperationAction(ISD::FP_EXTEND, MVT::f64, Expand);
    setOperationAction(ISD::FP_ROUND, MVT::f32, Expand);

    // MicroBlaze has no f64 hardware; expand all f64 ops to soft-float
    // libcalls. Without these explicit Expand+libcall registrations, type
    // softening would call makeLibCall without a registered impl and crash with
    // "unsupported library call operation".
    for (ISD::NodeType Op :
         {ISD::FADD,    ISD::FSUB,      ISD::FMUL,       ISD::FDIV,
          ISD::FREM,    ISD::FMA,       ISD::FNEG,       ISD::FABS,
          ISD::FSQRT,   ISD::FSIN,      ISD::FCOS,       ISD::FPOW,
          ISD::FLOG,    ISD::FLOG2,     ISD::FLOG10,     ISD::FEXP,
          ISD::FEXP2,   ISD::FRINT,     ISD::FNEARBYINT, ISD::FCEIL,
          ISD::FFLOOR,  ISD::FTRUNC,    ISD::FROUND,     ISD::FMINNUM,
          ISD::FMAXNUM, ISD::FCOPYSIGN, ISD::FLDEXP})
      setOperationAction(Op, MVT::f64, Expand);
    setLibcallImpl(RTLIB::ADD_F64, RTLIB::impl___adddf3);
    setLibcallImpl(RTLIB::SUB_F64, RTLIB::impl___subdf3);
    setLibcallImpl(RTLIB::MUL_F64, RTLIB::impl___muldf3);
    setLibcallImpl(RTLIB::DIV_F64, RTLIB::impl___divdf3);
    setLibcallImpl(RTLIB::FPEXT_F32_F64, RTLIB::impl___extendsfdf2);
    setLibcallImpl(RTLIB::FPROUND_F64_F32, RTLIB::impl___truncdfsf2);
    setLibcallImpl(RTLIB::FPTOSINT_F64_I32, RTLIB::impl___fixdfsi);
    setLibcallImpl(RTLIB::FPTOSINT_F64_I64, RTLIB::impl___fixdfdi);
    setLibcallImpl(RTLIB::FPTOUINT_F64_I32, RTLIB::impl___fixunsdfsi);
    setLibcallImpl(RTLIB::FPTOUINT_F64_I64, RTLIB::impl___fixunsdfdi);
    setLibcallImpl(RTLIB::SINTTOFP_I32_F64, RTLIB::impl___floatsidf);
    setLibcallImpl(RTLIB::SINTTOFP_I64_F64, RTLIB::impl___floatdidf);
    setLibcallImpl(RTLIB::UINTTOFP_I32_F64, RTLIB::impl___floatunsidf);
    setLibcallImpl(RTLIB::UINTTOFP_I64_F64, RTLIB::impl___floatundidf);
    setLibcallImpl(RTLIB::OEQ_F64, RTLIB::impl___eqdf2);
    setLibcallImpl(RTLIB::UNE_F64, RTLIB::impl___nedf2);
    setLibcallImpl(RTLIB::OLT_F64, RTLIB::impl___ltdf2);
    setLibcallImpl(RTLIB::OLE_F64, RTLIB::impl___ledf2);
    setLibcallImpl(RTLIB::OGT_F64, RTLIB::impl___gtdf2);
    setLibcallImpl(RTLIB::OGE_F64, RTLIB::impl___gedf2);
    setLibcallImpl(RTLIB::UO_F64, RTLIB::impl___unorddf2);
    // SETCC on f32 produces i1 but FCMP produces float 0/1; the conversion
    // is non-trivial, so expand SETCC and handle comparisons through BR_CC /
    // SELECT_CC custom lowering instead.
    setOperationAction(ISD::SETCC, MVT::f32, Expand);
    // BR_CC and SELECT_CC for f32 are custom-lowered to BR_CC_FP / SELECT_CC_FP
    // which ISelDAGToDAG / EmitInstrWithCustomInserter expand to FCMP + branch.
    setOperationAction(ISD::BR_CC, MVT::f32, Custom);
    // SELECT_CC result may be i32 (select int on float cmp) or f32 (float
    // ternary).
    setOperationAction(ISD::SELECT_CC, MVT::f32, Custom);
    setOperationAction(ISD::SELECT, MVT::f32, Expand);
    // MicroBlaze has no separate FPU load/store instructions; float values
    // are held in the same physical registers as integers.  Lower f32
    // load/store to i32 load/store + BITCAST so existing integer load/store
    // patterns apply.
    setOperationAction(ISD::LOAD, MVT::f32, Custom);
    setOperationAction(ISD::STORE, MVT::f32, Custom);
  }

  // Lower global addresses, external symbols, and constant pool entries
  // via the Wrapper node → ADDIK rD, r0, symbol.
  setOperationAction(ISD::GlobalAddress, MVT::i32, Custom);
  setOperationAction(ISD::ExternalSymbol, MVT::i32, Custom);
  setOperationAction(ISD::ConstantPool, MVT::i32, Custom);
  setOperationAction(ISD::BlockAddress, MVT::i32, Custom);

  // Indirect branch (computed goto / blockaddress / jump-table dispatch)
  // selects to BRAD (absolute register branch).
  setOperationAction(ISD::BRIND, MVT::Other, Legal);

  // Conditional branches: custom-lower BR_CC; BRCOND expands to BR_CC first.
  setOperationAction(ISD::BR_CC, MVT::i32, Custom);
  setOperationAction(ISD::BRCOND, MVT::Other, Expand);
  // SELECT expands to SELECT_CC. SELECT_CC is custom-lowered to
  // MicroBlazeISD::SELECT_CC which is expanded to a diamond CFG by
  // EmitInstrWithCustomInserter. Making SELECT_CC=Custom breaks the
  // SELECT→SELECT_CC→SETCC→SELECT_CC expansion cycle.
  setOperationAction(ISD::SELECT, MVT::i32, Expand);
  setOperationAction(ISD::SELECT_CC, MVT::i32, Custom);
  // With +pattern-compare, PCMPEQ/PCMPNE handle EQ/NE as integer 0/1.
  // Other conditions fall back to SELECT_CC(LHS, RHS, 1, 0, CC).
  setOperationAction(ISD::SETCC, MVT::i32,
                     STI.hasPatternCompare() ? Custom : Expand);

  // CLZ is available under +pattern-compare (UG984 §5 Fig 83, v8.10.a+).
  // CTTZ and CTPOP have no hardware instruction.
  // Guard CTLZ_ZERO_POISON too: llvm.ctlz(x, i1 true) generates that node,
  // which defaults to Legal and leaks the clz instruction without the feature.
  if (!STI.hasPatternCompare()) {
    setOperationAction(ISD::CTLZ, MVT::i32, Expand);
    setOperationAction(ISD::CTLZ_ZERO_POISON, MVT::i32, Expand);
  }
  setOperationAction(ISD::CTTZ, MVT::i32, Expand);
  setOperationAction(ISD::CTPOP, MVT::i32, Expand);

  // abs(x): no native instruction and no conditional-move, so the default
  // SRA+XOR+SUB expansion always runs unconditionally (3 instructions).
  // Custom-lower to a branch diamond (BGTID + conditional RSUBK) instead;
  // the negation executes only on the ≤0 path, saving ~1.5 insns/iteration
  // on typical abs workloads and matching GCC's strategy for this target.
  setOperationAction(ISD::ABS, MVT::i32, Custom);

  // bswap32 lowering: SWAPB is a full 32-bit byte reversal (ABCD→DCBA), so it
  // implements bswap32 in a single instruction.  Legal with tablegen pattern
  // whenever +swapb is available (+reorder implies +swapb).
  // Without +swapb: Expand → shift/or sequence.
  if (STI.hasSwapByte())
    setOperationAction(ISD::BSWAP, MVT::i32, Legal);
  else
    setOperationAction(ISD::BSWAP, MVT::i32, Expand);
  setOperationAction(ISD::ROTL, MVT::i32, Expand);
  setOperationAction(ISD::ROTR, MVT::i32, Expand);
  setOperationAction(ISD::BITREVERSE, MVT::i32, Expand);

  // 64-bit shifts decomposed into 32-bit pairs.
  setOperationAction(ISD::SHL_PARTS, MVT::i32, Expand);
  setOperationAction(ISD::SRL_PARTS, MVT::i32, Expand);
  setOperationAction(ISD::SRA_PARTS, MVT::i32, Expand);

  // Varargs: VASTART and VAARG are custom-lowered. VACOPY and VAEND are trivial
  // for a pointer-only va_list and can be Expanded (VACOPY → store, VAEND →
  // no-op).
  setOperationAction(ISD::VASTART, MVT::Other, Custom);
  setOperationAction(ISD::VAARG, MVT::Other, Custom);
  setOperationAction(ISD::VACOPY, MVT::Other, Expand);
  setOperationAction(ISD::VAEND, MVT::Other, Expand);

  // Jump tables: materialise the table address via the Wrapper node and let the
  // generic legalizer expand BR_JT into (load table[index]) + BRIND.  Static
  // model uses EK_BlockAddress entries (absolute MBB addresses,
  // R_MICROBLAZE_32). Keep LLVM's default 4-entry minimum (dense switches below
  // that stay trees).
  setOperationAction(ISD::JumpTable, MVT::i32, Custom);
  setOperationAction(ISD::BR_JT, MVT::Other, Expand);

  // Memory barriers: ISD::ATOMIC_FENCE → MBAR 1 (data-side barrier, UG984 §2).
  // MBAR 0 would also clear the BTC; MBAR 1 avoids that cost.
  setOperationAction(ISD::ATOMIC_FENCE, MVT::Other, Custom);

  // __builtin_frame_address / __builtin_return_address.
  // MicroBlaze has no dedicated frame-pointer register, so FRAMEADDR returns
  // R1 (the stack pointer) for depth 0.  RETURNADDR reads R15 (the hardware
  // link register) at function entry.  Depth > 0 requires stack walking,
  // which in turn requires saving a frame chain; unsupported.
  setOperationAction(ISD::FRAMEADDR,  MVT::i32, Custom);
  setOperationAction(ISD::RETURNADDR, MVT::i32, Custom);

  // STACKSAVE/STACKRESTORE: expand to copies of R1 (the stack pointer).
  // The generic SelectionDAGLegalizer expansion uses the register registered
  // by setStackPointerRegisterToSaveRestore() above.  Without this Expand
  // annotation the ops default to Legal and ISel crashes with "Cannot select".
  setOperationAction(ISD::STACKSAVE,    MVT::Other, Expand);
  setOperationAction(ISD::STACKRESTORE, MVT::Other, Expand);
  // DYNAMIC_STACKALLOC: used for VLAs where the size is a runtime value.
  // The generic ExpandDYNAMIC_STACKALLOC in SelectionDAGLegalize subtracts
  // the (rounded) size from R1 and returns the new SP as the allocation base.
  setOperationAction(ISD::DYNAMIC_STACKALLOC, MVT::i32,  Expand);

  setMinFunctionAlignment(Align(4));
}

bool MicroBlazeTargetLowering::isIntDivCheap(EVT VT, AttributeList Attr) const {
  // Always false: IDIV is a 34-cycle blocking instruction; multiply-by-magic
  // sequences using MULH are ~3 cycles.  Returning false lets the DAGCombiner
  // strength-reduce constant divisors when +multiply-high is available.
  // For variable divisors the combiner can't precompute the magic number, so
  // SDIV/UDIV remain and get selected to IDIV/IDIVU.
  return false;
}

const char *MicroBlazeTargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (static_cast<MicroBlazeISD::NodeType>(Opcode)) {
  case MicroBlazeISD::FIRST_NUMBER:
    break;
  case MicroBlazeISD::RET_FLAG:
    return "MicroBlazeISD::RET_FLAG";
  case MicroBlazeISD::INTR_RET:
    return "MicroBlazeISD::INTR_RET";
  case MicroBlazeISD::CALL:
    return "MicroBlazeISD::CALL";
  case MicroBlazeISD::Wrapper:
    return "MicroBlazeISD::Wrapper";
  case MicroBlazeISD::GOT_LOAD:
    return "MicroBlazeISD::GOT_LOAD";
  case MicroBlazeISD::BR_CC:
    return "MicroBlazeISD::BR_CC";
  case MicroBlazeISD::SELECT_CC:
    return "MicroBlazeISD::SELECT_CC";
  case MicroBlazeISD::BR_CC_CMP:
    return "MicroBlazeISD::BR_CC_CMP";
  case MicroBlazeISD::SELECT_CC_CMP:
    return "MicroBlazeISD::SELECT_CC_CMP";
  case MicroBlazeISD::BR_CC_CMPU:
    return "MicroBlazeISD::BR_CC_CMPU";
  case MicroBlazeISD::SELECT_CC_CMPU:
    return "MicroBlazeISD::SELECT_CC_CMPU";
  case MicroBlazeISD::BR_CC_FP:
    return "MicroBlazeISD::BR_CC_FP";
  case MicroBlazeISD::SELECT_CC_FP:
    return "MicroBlazeISD::SELECT_CC_FP";
  case MicroBlazeISD::PCMPEQ:
    return "MicroBlazeISD::PCMPEQ";
  case MicroBlazeISD::PCMPNE:
    return "MicroBlazeISD::PCMPNE";
  case MicroBlazeISD::ADDC:
    return "MicroBlazeISD::ADDC";
  case MicroBlazeISD::ADDE:
    return "MicroBlazeISD::ADDE";
  case MicroBlazeISD::SUBC:
    return "MicroBlazeISD::SUBC";
  case MicroBlazeISD::SUBE:
    return "MicroBlazeISD::SUBE";
  case MicroBlazeISD::ABS:
    return "MicroBlazeISD::ABS";
  }
  return nullptr;
}

//===----------------------------------------------------------------------===//
// Frame and return address lowering
//===----------------------------------------------------------------------===//

SDValue MicroBlazeTargetLowering::LowerFRAMEADDR(SDValue Op,
                                                  SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MFI.setFrameAddressIsTaken(true);

  unsigned Depth = Op.getConstantOperandVal(0);
  if (Depth > 0)
    // MicroBlaze has no dedicated frame-pointer register so walking the frame
    // chain is not supported.  Callers that need depth > 0 must use a
    // target with a saved-FP convention.
    report_fatal_error(
        "MicroBlaze: __builtin_frame_address with depth > 0 is not supported");

  EVT VT = Op.getValueType();
  SDLoc DL(Op);
  // Without a dedicated frame pointer, R1 (the stack pointer) is the closest
  // approximation.  It points to the bottom of the current frame immediately
  // after the prologue, which is the frame base for leaf functions.
  Register FP = Subtarget.getRegisterInfo()->getFrameRegister(MF);
  return DAG.getCopyFromReg(DAG.getEntryNode(), DL, FP, VT);
}

SDValue MicroBlazeTargetLowering::LowerRETURNADDR(SDValue Op,
                                                   SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MFI.setReturnAddressIsTaken(true);

  unsigned Depth = Op.getConstantOperandVal(0);
  if (Depth > 0)
    // Unwinding through multiple frames requires a saved frame-pointer chain,
    // which MicroBlaze does not maintain by default.
    report_fatal_error(
        "MicroBlaze: __builtin_return_address with depth > 0 is not supported");

  EVT VT = Op.getValueType();
  SDLoc DL(Op);
  // R15 is the hardware link register; it holds the return address at function
  // entry.  Anchoring the copy to getEntryNode() captures the entry value
  // before any calls inside the function overwrite R15.
  return DAG.getCopyFromReg(DAG.getEntryNode(), DL, MicroBlaze::R15, VT);
}

//===----------------------------------------------------------------------===//
// Custom lowering
//===----------------------------------------------------------------------===//

SDValue MicroBlazeTargetLowering::LowerOperation(SDValue Op,
                                                 SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  case ISD::GlobalAddress:
    return LowerGlobalAddress(Op, DAG);
  case ISD::ExternalSymbol:
    return LowerExternalSymbol(Op, DAG);
  case ISD::ConstantPool:
    return LowerConstantPool(Op, DAG);
  case ISD::BlockAddress:
    return LowerBlockAddress(Op, DAG);
  case ISD::JumpTable:
    return LowerJumpTable(Op, DAG);
  case ISD::BR_CC:
    return LowerBR_CC(Op, DAG);
  case ISD::SELECT_CC:
    return LowerSELECT_CC(Op, DAG);
  case ISD::SETCC:
    return LowerSETCC(Op, DAG);
  case ISD::SHL:
  case ISD::SRL:
  case ISD::SRA:
    return LowerShift(Op, DAG);
  case ISD::VASTART:
    return LowerVASTART(Op, DAG);
  case ISD::VAARG:
    return LowerVAARG(Op, DAG);
  case ISD::ATOMIC_FENCE:
    return LowerATOMIC_FENCE(Op, DAG);
  case ISD::LOAD:
    return LowerFP32Load(Op, DAG);
  case ISD::STORE:
    return LowerFP32Store(Op, DAG);
  case ISD::ABS:
    return LowerABS(Op, DAG);
  case ISD::UADDO:
    return LowerUADDO(Op, DAG);
  case ISD::USUBO:
    return LowerUSUBO(Op, DAG);
  case ISD::UADDO_CARRY:
    return LowerUADDO_CARRY(Op, DAG);
  case ISD::USUBO_CARRY:
    return LowerUSUBO_CARRY(Op, DAG);
  case ISD::FRAMEADDR:
    return LowerFRAMEADDR(Op, DAG);
  case ISD::RETURNADDR:
    return LowerRETURNADDR(Op, DAG);
  default:
    llvm_unreachable("Unexpected custom lowering");
  }
}

// MicroBlaze has no separate FPU load/store instructions: float values live
// in the same physical registers as integers.  Represent f32 load/store as
// i32 load/store followed by BITCAST so existing i32 memory patterns apply.
SDValue MicroBlazeTargetLowering::LowerFP32Load(SDValue Op,
                                                SelectionDAG &DAG) const {
  auto *LD = cast<LoadSDNode>(Op);
  SDLoc DL(Op);
  SDValue I32Val = DAG.getLoad(MVT::i32, DL, LD->getChain(), LD->getBasePtr(),
                               LD->getMemOperand());
  SDValue F32Val = DAG.getNode(ISD::BITCAST, DL, MVT::f32, I32Val);
  return DAG.getMergeValues({F32Val, I32Val.getValue(1)}, DL);
}

SDValue MicroBlazeTargetLowering::LowerFP32Store(SDValue Op,
                                                 SelectionDAG &DAG) const {
  auto *ST = cast<StoreSDNode>(Op);
  SDLoc DL(Op);
  SDValue I32Val = DAG.getNode(ISD::BITCAST, DL, MVT::i32, ST->getValue());
  return DAG.getStore(ST->getChain(), DL, I32Val, ST->getBasePtr(),
                      ST->getMemOperand());
}

SDValue MicroBlazeTargetLowering::LowerSETCC(SDValue Op,
                                             SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue LHS = Op.getOperand(0);
  SDValue RHS = Op.getOperand(1);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(2))->get();

  // PCMPEQ/PCMPNE produce an integer 0/1 result directly (UG984 §5 Figs
  // 116-118).
  if (CC == ISD::SETEQ)
    return DAG.getNode(MicroBlazeISD::PCMPEQ, DL, MVT::i32, LHS, RHS);
  if (CC == ISD::SETNE)
    return DAG.getNode(MicroBlazeISD::PCMPNE, DL, MVT::i32, LHS, RHS);

  // Other conditions: lower as SELECT_CC(LHS, RHS, 1, 0, CC) which routes
  // through LowerSELECT_CC → CMP/CMPU pseudo.
  SDValue One = DAG.getConstant(1, DL, MVT::i32);
  SDValue Zero = DAG.getConstant(0, DL, MVT::i32);
  return DAG.getNode(ISD::SELECT_CC, DL, MVT::i32, LHS, RHS, One, Zero,
                     DAG.getCondCode(CC));
}

SDValue MicroBlazeTargetLowering::LowerABS(SDValue Op,
                                           SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue X = Op.getOperand(0);
  // Emit MicroBlazeISD::ABS, matched by ABS_PSEUDO and expanded by
  // EmitInstrWithCustomInserter to a BGTID diamond: the RSUBK negation
  // only executes on the fall-through (x ≤ 0) path.
  return DAG.getNode(MicroBlazeISD::ABS, DL, MVT::i32, X);
}

SDValue MicroBlazeTargetLowering::LowerSELECT_CC(SDValue Op,
                                                 SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue LHS = Op.getOperand(0);
  SDValue RHS = Op.getOperand(1);
  SDValue TrueV = Op.getOperand(2);
  SDValue FalseV = Op.getOperand(3);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(4))->get();

  // Float comparison: emit SELECT_CC_FP expanded by
  // EmitInstrWithCustomInserter.
  if (LHS.getValueType() == MVT::f32) {
    SDValue CCVal = DAG.getConstant(CC, DL, MVT::i32);
    return DAG.getNode(MicroBlazeISD::SELECT_CC_FP, DL, Op.getValueType(),
                       TrueV, FalseV, CCVal, LHS, RHS);
  }

  // Integer comparison selecting between f32 values: UINT_TO_FP expansion
  // can produce SELECT_CC(i32_cmp, f32_true, f32_false).  Since f32 and i32
  // share the same GPRs, bitcast to i32, select, then bitcast back.
  EVT ResVT = Op.getValueType();
  bool ResIsFloat = ResVT == MVT::f32;
  SDValue TV = ResIsFloat ? DAG.getBitcast(MVT::i32, TrueV) : TrueV;
  SDValue FV = ResIsFloat ? DAG.getBitcast(MVT::i32, FalseV) : FalseV;

  SDValue Sel;
  if (Subtarget.hasPatternCompare()) {
    bool IsUnsignedIneq = (CC == ISD::SETUGT || CC == ISD::SETUGE ||
                           CC == ISD::SETULT || CC == ISD::SETULE);
    SDValue CCVal = DAG.getConstant(CC, DL, MVT::i32);
    unsigned Opc = IsUnsignedIneq ? MicroBlazeISD::SELECT_CC_CMPU
                                  : MicroBlazeISD::SELECT_CC_CMP;
    Sel = DAG.getNode(Opc, DL, MVT::i32, TV, FV, CCVal, LHS, RHS);
  } else {
    // Fallback: subtract and branch on sign/zero.
    SDValue Diff = DAG.getNode(ISD::SUB, DL, MVT::i32, LHS, RHS);
    SDValue CCVal = DAG.getConstant(CC, DL, MVT::i32);
    Sel = DAG.getNode(MicroBlazeISD::SELECT_CC, DL, MVT::i32, TV, FV, CCVal,
                      Diff);
  }
  return ResIsFloat ? DAG.getBitcast(MVT::f32, Sel) : Sel;
}

// Map an f32 ISD::CondCode to the primary FCMP machine opcode and an optional
// secondary opcode for "ordered primary OR unordered" conditions.
// Returns false if the condition is not representable with hardware fcmp.
//
// Unordered conditions (SETULT, SETULE, etc.) decompose as:
//   (ordered-op) OR (fcmp.un)  →  OR the two 0/1 float results, then BNEID.
// SETO (ordered, i.e., neither is NaN) uses FCMP_UN + BEQID (invert).
namespace llvm {
bool getFCmpOpcodes(ISD::CondCode CC, unsigned &Opc1, unsigned &Opc2,
                    bool &Invert) {
  Opc2 = 0;
  Invert = false;
  switch (CC) {
  // Direct ordered conditions → one FCMP instruction.
  // UG984 §5: fcmp.XX rD, rA, rB has reversed operand semantics for inequality
  // comparisons — fcmp.lt fires when rB < rA, fcmp.gt when rA < rB, etc.
  // With rA=LHS, rB=RHS we therefore swap LT↔GT and LE↔GE so the hardware
  // condition matches the DAG condition code.
  case ISD::SETOEQ:
    Opc1 = MicroBlaze::FCMP_EQ;
    return true;
  case ISD::SETONE:
    Opc1 = MicroBlaze::FCMP_NE;
    return true;
  case ISD::SETOLT:
    Opc1 = MicroBlaze::FCMP_GT;
    return true; // GT fires when LHS < RHS
  case ISD::SETOGT:
    Opc1 = MicroBlaze::FCMP_LT;
    return true; // LT fires when LHS > RHS
  case ISD::SETOLE:
    Opc1 = MicroBlaze::FCMP_GE;
    return true; // GE fires when LHS <= RHS
  case ISD::SETOGE:
    Opc1 = MicroBlaze::FCMP_LE;
    return true; // LE fires when LHS >= RHS
  // Unordered check → fcmp.un directly.
  case ISD::SETUO:
    Opc1 = MicroBlaze::FCMP_UN;
    return true;
  // Ordered check (neither is NaN) → fcmp.un + inverted branch.
  case ISD::SETO:
    Opc1 = MicroBlaze::FCMP_UN;
    Invert = true;
    return true;
  // Unordered variants: ordered-op OR fcmp.un.
  case ISD::SETUEQ:
    Opc1 = MicroBlaze::FCMP_EQ;
    Opc2 = MicroBlaze::FCMP_UN;
    return true;
  case ISD::SETUNE:
    Opc1 = MicroBlaze::FCMP_NE;
    Opc2 = MicroBlaze::FCMP_UN;
    return true;
  case ISD::SETULT:
    Opc1 = MicroBlaze::FCMP_GT;
    Opc2 = MicroBlaze::FCMP_UN;
    return true;
  case ISD::SETUGT:
    Opc1 = MicroBlaze::FCMP_LT;
    Opc2 = MicroBlaze::FCMP_UN;
    return true;
  case ISD::SETULE:
    Opc1 = MicroBlaze::FCMP_GE;
    Opc2 = MicroBlaze::FCMP_UN;
    return true;
  case ISD::SETUGE:
    Opc1 = MicroBlaze::FCMP_LE;
    Opc2 = MicroBlaze::FCMP_UN;
    return true;
  // Integer (NaN-unconcerned) conditions: emitted when nofpclass constraints
  // let the DAGCombiner prove neither operand is NaN.  Since NaN is impossible
  // these map identically to their ordered (SETO*) equivalents.
  case ISD::SETEQ:
    Opc1 = MicroBlaze::FCMP_EQ;
    return true;
  case ISD::SETNE:
    Opc1 = MicroBlaze::FCMP_NE;
    return true;
  case ISD::SETLT:
    Opc1 = MicroBlaze::FCMP_GT;
    return true;
  case ISD::SETGT:
    Opc1 = MicroBlaze::FCMP_LT;
    return true;
  case ISD::SETLE:
    Opc1 = MicroBlaze::FCMP_GE;
    return true;
  case ISD::SETGE:
    Opc1 = MicroBlaze::FCMP_LE;
    return true;
  default:
    return false;
  }
}
} // namespace llvm

// Return true if CMP operands need to be swapped (emitting CMP rd, RHS, LHS
// instead of CMP rd, LHS, RHS) for this condition code.
// CMP rD, rA, rB: rD = rB - rA, rD[31]=1 iff rA > rB signed.
// Edge case: when rB-rA = 0x80000000, arithmetic bit31=1 but the comparison
// flag wants bit31=0, so CMP zeroes it → rD=0 → wrong branch.
// Swapping so rD = rA - rA = LHS - RHS ensures the arithmetic and comparison
// bit31 always agree for SETLT/SETGE.
static bool mbCmpNeedsSwap(ISD::CondCode CC) {
  return CC == ISD::SETLT || CC == ISD::SETGE;
}

static bool mbCmpuNeedsSwap(ISD::CondCode CC) {
  return CC == ISD::SETULT || CC == ISD::SETUGE;
}

// Branch opcode after CMP (with swapped operands for SETLT/SETGE).
// SETLT/SETGE (swapped): rD = LHS-RHS, rD[31]=1 iff LHS<RHS.
//   SETLT → fire when rD<0 → BLTID
//   SETGE → fire when rD≥0 → BGEID
// SETGT/SETLE (natural): rD = RHS-LHS, rD[31]=1 iff LHS>RHS.
//   SETGT → fire when rD<0 → BLTID
//   SETLE → fire when rD≥0 → BGEID
static unsigned getMBCmpBranchOpcodeForCC(ISD::CondCode CC) {
  switch (CC) {
  case ISD::SETEQ:
    return MicroBlaze::BEQID;
  case ISD::SETNE:
    return MicroBlaze::BNEID;
  case ISD::SETLT:
    return MicroBlaze::BLTID;
  case ISD::SETLE:
    return MicroBlaze::BGEID;
  case ISD::SETGT:
    return MicroBlaze::BLTID;
  case ISD::SETGE:
    return MicroBlaze::BGEID;
  default:
    llvm_unreachable("Expected signed CC for CMP branch");
  }
}

// Branch opcode after CMPU (with swapped operands for SETULT/SETUGE).
// SETULT/SETUGE (swapped): rD = LHS-RHS, rD[31]=1 iff LHS<RHS unsigned.
//   SETULT → BLTID;  SETUGE → BGEID
// SETUGT/SETULE (natural): rD = RHS-LHS, rD[31]=1 iff LHS>RHS unsigned.
//   SETUGT → BLTID;  SETULE → BGEID
static unsigned getMBCmpuBranchOpcodeForCC(ISD::CondCode CC) {
  switch (CC) {
  case ISD::SETUGT:
    return MicroBlaze::BLTID;
  case ISD::SETULT:
    return MicroBlaze::BLTID;
  case ISD::SETUGE:
    return MicroBlaze::BGEID;
  case ISD::SETULE:
    return MicroBlaze::BGEID;
  default:
    llvm_unreachable("Expected unsigned inequality CC for CMPU branch");
  }
}

// Map ISD::CondCode to the MicroBlaze zero-test branch opcode.
// MicroBlaze conditional branches compare rA to zero; Diff = LHS - RHS is rA.
static unsigned getMBBranchOpcodeForCC(ISD::CondCode CC) {
  switch (CC) {
  case ISD::SETEQ:
    return MicroBlaze::BEQID;
  case ISD::SETNE:
    return MicroBlaze::BNEID;
  case ISD::SETLT:
    return MicroBlaze::BLTID;
  case ISD::SETLE:
    return MicroBlaze::BLEID;
  case ISD::SETGT:
    return MicroBlaze::BGTID;
  case ISD::SETGE:
    return MicroBlaze::BGEID;
  // Unsigned: signed branch approximation (correct when values fit in
  // [0,2^31)).
  case ISD::SETULT:
    return MicroBlaze::BLTID;
  case ISD::SETULE:
    return MicroBlaze::BLEID;
  case ISD::SETUGT:
    return MicroBlaze::BGTID;
  case ISD::SETUGE:
    return MicroBlaze::BGEID;
  default:
    llvm_unreachable("Unsupported condition code for MicroBlaze SELECT_CC");
  }
}

// Shared helper: build the diamond CFG for a conditional select.
// headMBB gets a conditional branch to sinkMBB (true path); fall-through is
// the false path.  BranchReg is the register to test; BrOpc is the opcode.
//
//   headMBB:
//     BrOpc BranchReg, sinkMBB   // condition TRUE → branch to sinkMBB
//   copy0MBB:                    // false-value staging block (empty)
//   sinkMBB:
//     dst = phi [FalseV, copy0MBB], [TrueV, headMBB]
static MachineBasicBlock *
emitSelectCCDiamond(MachineInstr &MI, MachineBasicBlock *BB, unsigned BrOpc,
                    Register BranchReg, Register DstReg, Register TrueReg,
                    Register FalseReg) {
  DebugLoc DL = MI.getDebugLoc();
  MachineFunction *MF = BB->getParent();
  const TargetInstrInfo &TII = *MF->getSubtarget().getInstrInfo();
  const BasicBlock *LLVM_BB = BB->getBasicBlock();
  MachineFunction::iterator It = ++BB->getIterator();

  MachineBasicBlock *headMBB = BB;
  MachineBasicBlock *copy0MBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *sinkMBB = MF->CreateMachineBasicBlock(LLVM_BB);
  MF->insert(It, copy0MBB);
  MF->insert(It, sinkMBB);

  sinkMBB->splice(sinkMBB->begin(), headMBB, std::next(MI.getIterator()),
                  headMBB->end());
  sinkMBB->transferSuccessorsAndUpdatePHIs(headMBB);

  BuildMI(headMBB, DL, TII.get(BrOpc)).addReg(BranchReg).addMBB(sinkMBB);
  headMBB->addSuccessor(copy0MBB);
  headMBB->addSuccessor(sinkMBB);
  copy0MBB->addSuccessor(sinkMBB);

  BuildMI(*sinkMBB, sinkMBB->begin(), DL, TII.get(TargetOpcode::PHI), DstReg)
      .addReg(FalseReg)
      .addMBB(copy0MBB)
      .addReg(TrueReg)
      .addMBB(headMBB);

  MI.eraseFromParent();
  return sinkMBB;
}

// Expand SELECT_CC_PSEUDO / SELECT_CC_CMP_PSEUDO / SELECT_CC_CMPU_PSEUDO.
//
// SELECT_CC_PSEUDO operands:      dst(0) TrueV(1) FalseV(2) CC_imm(3) Diff(4)
// SELECT_CC_CMP_PSEUDO operands:  dst(0) TrueV(1) FalseV(2) CC_imm(3) LHS(4)
// RHS(5) SELECT_CC_CMPU_PSEUDO operands: dst(0) TrueV(1) FalseV(2) CC_imm(3)
// LHS(4) RHS(5)
MachineBasicBlock *MicroBlazeTargetLowering::EmitInstrWithCustomInserter(
    MachineInstr &MI, MachineBasicBlock *BB) const {
  // ABS_PSEUDO: abs(src) via a branch diamond.
  //
  //   headMBB:  BGTID src, sinkMBB   (delayed branch; delay slot filled by DSF)
  //   negMBB:   RSUBK dst_neg, src, R0   (0 - src = -src; runs only when src ≤
  //   0) sinkMBB:  dst = phi [dst_neg, negMBB], [src, headMBB]
  //
  // DSF's searchJoinBB moves the first safe instruction from sinkMBB into the
  // BGTID delay slot, hiding the lw→bgtid load-use stall and removing the
  // 2-cycle branch penalty on the taken path.
  if (MI.getOpcode() == MicroBlaze::ABS_PSEUDO) {
    DebugLoc DL = MI.getDebugLoc();
    MachineFunction *MF = BB->getParent();
    const TargetInstrInfo &TII = *MF->getSubtarget().getInstrInfo();
    const BasicBlock *LLVM_BB = BB->getBasicBlock();
    MachineFunction::iterator It = ++BB->getIterator();

    Register DstReg = MI.getOperand(0).getReg();
    Register SrcReg = MI.getOperand(1).getReg();

    MachineBasicBlock *headMBB = BB;
    MachineBasicBlock *negMBB = MF->CreateMachineBasicBlock(LLVM_BB);
    MachineBasicBlock *sinkMBB = MF->CreateMachineBasicBlock(LLVM_BB);
    MF->insert(It, negMBB);
    MF->insert(It, sinkMBB);

    sinkMBB->splice(sinkMBB->begin(), headMBB, std::next(MI.getIterator()),
                    headMBB->end());
    sinkMBB->transferSuccessorsAndUpdatePHIs(headMBB);

    Register NegReg =
        MF->getRegInfo().createVirtualRegister(&MicroBlaze::GPRRegClass);
    BuildMI(*sinkMBB, sinkMBB->begin(), DL, TII.get(TargetOpcode::PHI), DstReg)
        .addReg(NegReg)
        .addMBB(negMBB)
        .addReg(SrcReg)
        .addMBB(headMBB);

    // Emit delayed branch so DSF can pull an instruction from sinkMBB
    // (the join block) into the slot via searchJoinBB.
    BuildMI(headMBB, DL, TII.get(MicroBlaze::BGTID))
        .addReg(SrcReg)
        .addMBB(sinkMBB);
    headMBB->addSuccessor(negMBB);
    headMBB->addSuccessor(sinkMBB);

    // negMBB: negate src — executes only when src ≤ 0.
    // RSUBK rD, rA, rB = rD = rB - rA; with rB = R0 (hardwired 0): rD = -rA.
    BuildMI(*negMBB, negMBB->end(), DL, TII.get(MicroBlaze::RSUBK), NegReg)
        .addReg(SrcReg)
        .addReg(MicroBlaze::R0);
    negMBB->addSuccessor(sinkMBB);

    MI.eraseFromParent();
    return sinkMBB;
  }

  if (MI.getOpcode() == MicroBlaze::SELECT_CC_PSEUDO) {
    ISD::CondCode CC = static_cast<ISD::CondCode>(MI.getOperand(3).getImm());
    Register DiffReg = MI.getOperand(4).getReg();
    return emitSelectCCDiamond(
        MI, BB, getMBBranchOpcodeForCC(CC), DiffReg, MI.getOperand(0).getReg(),
        MI.getOperand(1).getReg(), MI.getOperand(2).getReg());
  }

  // SELECT_CC_FP_PSEUDO / SELECT_CC_FP_F32_PSEUDO:
  //   dst(0) TrueV(1) FalseV(2) CC_imm(3) LHS_f32(4) RHS_f32(5)
  // Emit FCMP (+ optional second FCMP + OR_ for unordered CCs), then diamond.
  if (MI.getOpcode() == MicroBlaze::SELECT_CC_FP_PSEUDO ||
      MI.getOpcode() == MicroBlaze::SELECT_CC_FP_F32_PSEUDO) {
    DebugLoc DL = MI.getDebugLoc();
    MachineFunction *MF = BB->getParent();
    const TargetInstrInfo &TII = *MF->getSubtarget().getInstrInfo();
    ISD::CondCode CC = static_cast<ISD::CondCode>(MI.getOperand(3).getImm());
    Register LHSReg = MI.getOperand(4).getReg();
    Register RHSReg = MI.getOperand(5).getReg();

    unsigned Opc1, Opc2;
    bool Invert;
    if (!getFCmpOpcodes(CC, Opc1, Opc2, Invert))
      llvm_unreachable("Unhandled f32 CC in SELECT_CC_FP pseudo");

    Register FlagReg =
        MF->getRegInfo().createVirtualRegister(&MicroBlaze::GPRRegClass);
    BuildMI(*BB, MI, DL, TII.get(Opc1), FlagReg).addReg(LHSReg).addReg(RHSReg);

    if (Opc2) {
      Register Flag2 =
          MF->getRegInfo().createVirtualRegister(&MicroBlaze::GPRRegClass);
      BuildMI(*BB, MI, DL, TII.get(Opc2), Flag2).addReg(LHSReg).addReg(RHSReg);
      Register OrReg =
          MF->getRegInfo().createVirtualRegister(&MicroBlaze::GPRRegClass);
      BuildMI(*BB, MI, DL, TII.get(MicroBlaze::OR_), OrReg)
          .addReg(FlagReg)
          .addReg(Flag2);
      FlagReg = OrReg;
    }

    unsigned BrOpc = Invert ? MicroBlaze::BEQID : MicroBlaze::BNEID;
    return emitSelectCCDiamond(
        MI, BB, BrOpc, FlagReg, MI.getOperand(0).getReg(),
        MI.getOperand(1).getReg(), MI.getOperand(2).getReg());
  }

  DebugLoc DL = MI.getDebugLoc();
  MachineFunction *MF = BB->getParent();
  const TargetInstrInfo &TII = *MF->getSubtarget().getInstrInfo();
  ISD::CondCode CC = static_cast<ISD::CondCode>(MI.getOperand(3).getImm());
  Register LHSReg = MI.getOperand(4).getReg();
  Register RHSReg = MI.getOperand(5).getReg();

  if (MI.getOpcode() == MicroBlaze::SELECT_CC_CMP_PSEUDO) {
    // SETEQ/SETNE: use RSUBK instead of CMP to avoid the false-zero edge case.
    //   CMP rD, LHS, RHS gives rD=0 when RHS-LHS=0x80000000 and LHS≤RHS (signed),
    //   e.g. LHS=INT_MIN, RHS=0 → rD=0 but they're not equal. RSUBK has no
    //   comparison-bit overwrite, so RSUBK(LHS,RHS)=0 iff LHS==RHS exactly.
    // SETLT/SETGE: swap CMP operands so rD = LHS-RHS (avoids the same edge case).
    bool UseRSub = (CC == ISD::SETEQ || CC == ISD::SETNE);
    bool Swap = !UseRSub && mbCmpNeedsSwap(CC);
    unsigned CmpOpc = UseRSub ? MicroBlaze::RSUBK : MicroBlaze::CMP;
    Register CmpReg =
        MF->getRegInfo().createVirtualRegister(&MicroBlaze::GPRRegClass);
    BuildMI(*BB, MI, DL, TII.get(CmpOpc), CmpReg)
        .addReg(Swap ? RHSReg : LHSReg)
        .addReg(Swap ? LHSReg : RHSReg);
    return emitSelectCCDiamond(MI, BB, getMBCmpBranchOpcodeForCC(CC), CmpReg,
                               MI.getOperand(0).getReg(),
                               MI.getOperand(1).getReg(),
                               MI.getOperand(2).getReg());
  }

  assert(MI.getOpcode() == MicroBlaze::SELECT_CC_CMPU_PSEUDO &&
         "Unknown custom-inserter pseudo");
  // SETULT/SETUGE: swap operands so CMPU computes LHS-RHS (avoids the same
  // 0x80000000 edge case as CMP).
  bool SwapU = mbCmpuNeedsSwap(CC);
  Register CmpuReg =
      MF->getRegInfo().createVirtualRegister(&MicroBlaze::GPRRegClass);
  BuildMI(*BB, MI, DL, TII.get(MicroBlaze::CMPU), CmpuReg)
      .addReg(SwapU ? RHSReg : LHSReg)
      .addReg(SwapU ? LHSReg : RHSReg);
  return emitSelectCCDiamond(MI, BB, getMBCmpuBranchOpcodeForCC(CC), CmpuReg,
                             MI.getOperand(0).getReg(),
                             MI.getOperand(1).getReg(),
                             MI.getOperand(2).getReg());
}

SDValue MicroBlazeTargetLowering::LowerATOMIC_FENCE(SDValue Op,
                                                    SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Chain = Op.getOperand(0);
  auto SSId = static_cast<SyncScope::ID>(Op.getConstantOperandVal(2));

  // A single-thread fence only constrains the compiler, not the hardware.
  if (SSId == SyncScope::SingleThread)
    return Chain;

  // mbar 1 = data-side barrier: drains all pending data-memory operations
  // before subsequent memory accesses can proceed (UG984 §2).  We use mbar 1
  // rather than mbar 0 to preserve the Branch Target Cache (mbar 0 clears it).
  SDValue Imm = DAG.getTargetConstant(1, DL, MVT::i32);
  return SDValue(
      DAG.getMachineNode(MicroBlaze::MBAR, DL, MVT::Other, {Imm, Chain}), 0);
}

//===----------------------------------------------------------------------===//
// Atomic LL/SC expansion (LWX / SWX)
//
// MicroBlaze provides hardware load-linked / store-conditional via LWX and SWX
// (AMD UG984 §5, opcodes 0x32 / 0x36).  LWX sets a reservation on the target
// address; SWX stores conditionally and clears the reservation, reporting the
// outcome through MSR[C] (bit 29 in MSB-first numbering, bit 2 as an integer):
//   MSR[C] = 0  on success
//   MSR[C] = 1  on failure (reservation stolen by interrupt or another core)
//
// This implementation follows the behaviour described in UG984 §5.
//===----------------------------------------------------------------------===//

// AtomicExpandPass calls these two hooks to build the LL/SC retry loop at IR
// level.  The loop structure is:
//
//   loop:
//     old = emitLoadLinked(addr)     ; LWX rD, rA, r0
//     <compute new value from old>
//     success = emitStoreConditional(new, addr)   ; SWX + MFS + ANDI
//     if (success != 0) goto loop    ; retry on reservation failure

TargetLowering::AtomicExpansionKind
MicroBlazeTargetLowering::shouldExpandAtomicCmpXchgInIR(
    const AtomicCmpXchgInst *AI) const {
  // LWX/SWX operate on 32-bit words; setMinCmpXchgSizeInBits(32) above
  // promotes narrower cmpxchg operations before this point.
  return AtomicExpansionKind::LLSC;
}

TargetLowering::AtomicExpansionKind
MicroBlazeTargetLowering::shouldExpandAtomicRMWInIR(
    const AtomicRMWInst *AI) const {
  // Expand all 32-bit atomicrmw operations to LWX/SWX LL/SC loops.
  // Sub-word (i8/i16) operations are promoted to i32 width with masking by
  // AtomicExpandPass using setMaxAtomicSizeInBitsSupported(32) above.
  return AtomicExpansionKind::LLSC;
}

Value *MicroBlazeTargetLowering::emitLoadLinked(IRBuilderBase &Builder,
                                                Type *ValueTy, Value *Addr,
                                                AtomicOrdering Ord) const {
  // LWX rD, rA, rB — load word exclusive; rB=r0 so effective address = rA+0.
  // Sets the hardware reservation bit on the memory address.
  // UG984 §5 (opcode 0x32, func=0x400).
  auto *I32Ty = Type::getInt32Ty(Builder.getContext());
  FunctionType *FTy = FunctionType::get(I32Ty, {Addr->getType()}, false);
  InlineAsm *IA = InlineAsm::get(FTy, "lwx $0, $1, r0", "=&r,r",
                                 /*hasSideEffects=*/true);
  return Builder.CreateCall(IA, {Addr});
}

Value *MicroBlazeTargetLowering::emitStoreConditional(
    IRBuilderBase &Builder, Value *Val, Value *Addr, AtomicOrdering Ord) const {
  // SWX rD, rA, rB — store word exclusive; rB=r0 so effective address = rA+0.
  // Clears reservation on success.  UG984 §5 (opcode 0x36, func=0x400):
  //   MSR[C] = 0  on success (reservation was still set)
  //   MSR[C] = 1  on failure (reservation was cleared externally)
  //
  // MFS rD, rMSR then reads the MSR.  ANDI isolates the carry bit at
  // MSR bit 29 (MSB-first numbering) = bit 2 in the 32-bit integer.
  //
  // AtomicExpandPass treats the return value as: 0 = success, non-zero = retry.
  // MSR[C]=0 (success) → andi result = 0 ✓
  // MSR[C]=1 (failure) → andi result = 4, non-zero → retry ✓
  //
  // All three instructions are in one asm block so the MSR read is
  // guaranteed to immediately follow the SWX with no intervening code.
  auto *I32Ty = Type::getInt32Ty(Builder.getContext());
  FunctionType *FTy = FunctionType::get(I32Ty, {I32Ty, Addr->getType()}, false);
  InlineAsm *IA =
      InlineAsm::get(FTy,
                     "swx $1, $2, r0\n\t" // store exclusive
                     "mfs $0, rmsr\n\t"   // read MSR into result register
                     "andi $0, $0, 4",    // isolate carry bit (bit 2 = MSR[C])
                     "=&r,r,r",
                     /*hasSideEffects=*/true);
  return Builder.CreateCall(IA, {Val, Addr});
}


SDValue MicroBlazeTargetLowering::LowerShift(SDValue Op,
                                             SelectionDAG &DAG) const {
  SDLoc DL(Op);
  EVT VT = Op.getValueType();

  // shl x, 1 → add x, x.  InstCombine canonicalizes (add x, x) to (shl x, 1)
  // at the IR level, so this is the common case for pointer/index doubling.
  // Emitting addk saves the __ashlsi3 call+return overhead (~10 instructions).
  if (Op.getOpcode() == ISD::SHL)
    if (auto *C = dyn_cast<ConstantSDNode>(Op.getOperand(1)))
      if (C->getZExtValue() == 1)
        return DAG.getNode(ISD::ADD, DL, VT, Op.getOperand(0),
                           Op.getOperand(0));

  RTLIB::Libcall LC;
  switch (Op.getOpcode()) {
  case ISD::SHL:
    LC = RTLIB::getSHL(MVT::i32);
    break;
  case ISD::SRL:
    LC = RTLIB::getSRL(MVT::i32);
    break;
  case ISD::SRA:
    LC = RTLIB::getSRA(MVT::i32);
    break;
  default:
    llvm_unreachable("Unexpected shift opcode");
  }
  SDValue Ops[] = {Op.getOperand(0), Op.getOperand(1)};
  MakeLibCallOptions CallOptions;
  return makeLibCall(DAG, LC, MVT::i32, Ops, CallOptions, DL).first;
}

SDValue MicroBlazeTargetLowering::LowerBR_CC(SDValue Op,
                                             SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Chain = Op.getOperand(0);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(1))->get();
  SDValue LHS = Op.getOperand(2);
  SDValue RHS = Op.getOperand(3);
  SDValue Dest = Op.getOperand(4);

  // Float comparison: emit BR_CC_FP which ISelDAGToDAG lowers to FCMP + branch.
  if (LHS.getValueType() == MVT::f32) {
    SDValue CCVal = DAG.getConstant(CC, DL, MVT::i32);
    return DAG.getNode(MicroBlazeISD::BR_CC_FP, DL, MVT::Other, Chain, CCVal,
                       LHS, RHS, Dest);
  }

  // Zero-test: SETEQ/SETNE against zero needs no comparison instruction.
  // MicroBlaze beqid/bneid compare the operand register directly against zero,
  // so both the cmp/rsubk paths below would emit a redundant instruction.
  if ((CC == ISD::SETEQ || CC == ISD::SETNE) && isNullConstant(RHS)) {
    SDValue CCVal = DAG.getConstant(CC, DL, MVT::i32);
    return DAG.getNode(MicroBlazeISD::BR_CC, DL, MVT::Other, Chain, CCVal, LHS,
                       Dest);
  }

  if (Subtarget.hasPatternCompare()) {
    bool IsUnsignedIneq = (CC == ISD::SETUGT || CC == ISD::SETUGE ||
                           CC == ISD::SETULT || CC == ISD::SETULE);
    SDValue CCVal = DAG.getConstant(CC, DL, MVT::i32);
    if (IsUnsignedIneq)
      // CMPU: bit31=1 iff LHS > RHS unsigned; branch opcodes reversed.
      return DAG.getNode(MicroBlazeISD::BR_CC_CMPU, DL, MVT::Other, Chain,
                         CCVal, LHS, RHS, Dest);
    // CMP: bit31=1 iff LHS < RHS signed (overflow-safe); branch opcodes direct.
    return DAG.getNode(MicroBlazeISD::BR_CC_CMP, DL, MVT::Other, Chain, CCVal,
                       LHS, RHS, Dest);
  }

  // Fallback (no pattern-compare unit): subtract and branch on sign/zero.
  // Note: signed overflow can produce wrong results for SETLT/GT/LE/GE.
  SDValue Diff = DAG.getNode(ISD::SUB, DL, MVT::i32, LHS, RHS);
  SDValue CCVal = DAG.getConstant(CC, DL, MVT::i32);
  return DAG.getNode(MicroBlazeISD::BR_CC, DL, MVT::Other, Chain, CCVal, Diff,
                     Dest);
}

SDValue MicroBlazeTargetLowering::LowerGlobalAddress(SDValue Op,
                                                     SelectionDAG &DAG) const {
  SDLoc DL(Op);
  auto *GAN = cast<GlobalAddressSDNode>(Op);

  if (isPositionIndependent()) {
    // PIC: load the symbol's address from the GOT via R20.
    // Emits: imm HI16; lwi rD, r20, LO16  with R_MICROBLAZE_GOT_64.
    SDValue GOTSym = DAG.getTargetGlobalAddress(
        GAN->getGlobal(), DL, MVT::i32, GAN->getOffset(), MicroBlazeII::MO_GOT);
    return DAG
        .getNode(MicroBlazeISD::GOT_LOAD, DL,
                 DAG.getVTList(MVT::i32, MVT::Other), DAG.getEntryNode(),
                 GOTSym)
        .getValue(0);
  }

  SDValue GAWrapper = DAG.getTargetGlobalAddress(GAN->getGlobal(), DL, MVT::i32,
                                                 GAN->getOffset());
  return DAG.getNode(MicroBlazeISD::Wrapper, DL, MVT::i32, GAWrapper);
}

SDValue MicroBlazeTargetLowering::LowerExternalSymbol(SDValue Op,
                                                      SelectionDAG &DAG) const {
  SDLoc DL(Op);
  const char *Sym = cast<ExternalSymbolSDNode>(Op)->getSymbol();

  if (isPositionIndependent()) {
    SDValue GOTSym =
        DAG.getTargetExternalSymbol(Sym, MVT::i32, MicroBlazeII::MO_GOT);
    return DAG
        .getNode(MicroBlazeISD::GOT_LOAD, DL,
                 DAG.getVTList(MVT::i32, MVT::Other), DAG.getEntryNode(),
                 GOTSym)
        .getValue(0);
  }

  SDValue ESWrapper = DAG.getTargetExternalSymbol(Sym, MVT::i32);
  return DAG.getNode(MicroBlazeISD::Wrapper, DL, MVT::i32, ESWrapper);
}

SDValue MicroBlazeTargetLowering::LowerConstantPool(SDValue Op,
                                                    SelectionDAG &DAG) const {
  SDLoc DL(Op);
  auto *CP = cast<ConstantPoolSDNode>(Op);
  SDValue CPAddr = DAG.getTargetConstantPool(CP->getConstVal(), MVT::i32,
                                             CP->getAlign(), CP->getOffset());
  return DAG.getNode(MicroBlazeISD::Wrapper, DL, MVT::i32, CPAddr);
}

SDValue MicroBlazeTargetLowering::LowerJumpTable(SDValue Op,
                                                 SelectionDAG &DAG) const {
  SDLoc DL(Op);
  auto *JT = cast<JumpTableSDNode>(Op);

  if (isPositionIndependent()) {
    // PIC: the table entries are MBB - .LJTI label differences (the default
    // EK_LabelDifference32 encoding), so the base must be materialised
    // position- independently as an offset from the GOT base: addik rD, r20,
    // .LJTI@GOTOFF (a link-time constant — no runtime relocation).
    SDValue JTI = DAG.getTargetJumpTable(JT->getIndex(), MVT::i32,
                                         MicroBlazeII::MO_GOTOFF);
    SDValue Off = DAG.getNode(MicroBlazeISD::Wrapper, DL, MVT::i32, JTI);
    SDValue GOTBase = DAG.getRegister(MicroBlaze::R20, MVT::i32);
    return DAG.getNode(ISD::ADD, DL, MVT::i32, GOTBase, Off);
  }

  // Static: materialise the absolute base via Wrapper → ADDIK rD, r0, .LJTI.
  SDValue JTI = DAG.getTargetJumpTable(JT->getIndex(), MVT::i32);
  return DAG.getNode(MicroBlazeISD::Wrapper, DL, MVT::i32, JTI);
}

SDValue MicroBlazeTargetLowering::LowerBlockAddress(SDValue Op,
                                                    SelectionDAG &DAG) const {
  SDLoc DL(Op);
  auto *BA = cast<BlockAddressSDNode>(Op);
  // Block addresses are function-local labels; materialise the absolute address
  // via the Wrapper node → ADDIK rD, r0, .Ltmp (R_MICROBLAZE_32), same path as
  // GlobalAddress in the static model.
  SDValue BAWrapper = DAG.getTargetBlockAddress(BA->getBlockAddress(), MVT::i32,
                                                BA->getOffset());
  return DAG.getNode(MicroBlazeISD::Wrapper, DL, MVT::i32, BAWrapper);
}

//===----------------------------------------------------------------------===//
// Formal argument lowering
//===----------------------------------------------------------------------===//

SDValue MicroBlazeTargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {

  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  MicroBlazeMachineFunctionInfo *FuncInfo =
      MF.getInfo<MicroBlazeMachineFunctionInfo>();

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());
  CCInfo.AnalyzeFormalArguments(Ins, CC_MicroBlaze);

  SmallVector<SDValue, 4> OutChains;

  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    CCValAssign &VA = ArgLocs[i];
    if (VA.isRegLoc()) {
      const TargetRegisterClass *RC = &MicroBlaze::GPRRegClass;
      Register VReg = MRI.createVirtualRegister(RC);
      MRI.addLiveIn(VA.getLocReg(), VReg);
      SDValue ArgValue = DAG.getCopyFromReg(Chain, DL, VReg, VA.getLocVT());
      InVals.push_back(ArgValue);
    } else {
      assert(VA.isMemLoc() && "Unexpected argument location");
      unsigned ObjSize = VA.getLocVT().getStoreSize();
      int FI = MFI.CreateFixedObject(ObjSize, VA.getLocMemOffset(), true);
      SDValue FIN = DAG.getFrameIndex(FI, MVT::i32);
      InVals.push_back(DAG.getLoad(VA.getValVT(), DL, Chain, FIN,
                                   MachinePointerInfo::getFixedStack(MF, FI)));
    }
  }

  // For vararg functions, spill unused argument registers (R5-R10) to a
  // register-save area inside the callee's own frame.  Following RISC-V
  // convention (RISCVISelLowering.cpp), the save area sits at a NEGATIVE
  // offset from the incoming SP so it does not overlap any variable in the
  // caller's frame:
  //
  //   incoming SP  ──────────────────────────────────────────────────────
  //                │  vararg register save area  │  NumFree × 4 bytes
  //                │  (va_list points here)      │  at [incomingSP - N]
  //   new SP  ─────┼─────────────────────────────┼──────────────────────
  //                │  LR save, locals, ...       │
  //
  // If all argument registers were consumed by named args (NumFree == 0)
  // the first vararg is already on the stack at CCInfo.getStackSize().
  if (IsVarArg) {
    static const MCPhysReg ArgRegs[] = {MicroBlaze::R5, MicroBlaze::R6,
                                        MicroBlaze::R7, MicroBlaze::R8,
                                        MicroBlaze::R9, MicroBlaze::R10};
    unsigned FirstFree = CCInfo.getFirstUnallocated(ArgRegs);
    unsigned NumFree = std::size(ArgRegs) - FirstFree;
    int VarArgsSaveSize = NumFree * 4;
    int64_t VaArgOffset;
    int VarFI;

    if (VarArgsSaveSize == 0) {
      // All register args consumed by named parameters; first vararg is on
      // the stack.  Create a dummy 4-byte fixed object so VarArgsFrameIndex
      // is valid.
      VaArgOffset = CCInfo.getStackSize();
      VarFI = MFI.CreateFixedObject(4, VaArgOffset, true);
    } else {
      // Negative offset: save area lives at the TOP of the callee's frame,
      // just below the incoming SP, not in the caller's address space.
      VaArgOffset = -(int64_t)VarArgsSaveSize;
      VarFI = MFI.CreateFixedObject(VarArgsSaveSize, VaArgOffset, true);

      SDValue FIN = DAG.getFrameIndex(VarFI, MVT::i32);
      for (unsigned i = 0; i < NumFree; ++i) {
        Register VReg = MRI.createVirtualRegister(&MicroBlaze::GPRRegClass);
        MRI.addLiveIn(ArgRegs[FirstFree + i], VReg);
        SDValue Val = DAG.getCopyFromReg(Chain, DL, VReg, MVT::i32);
        SDValue Off = DAG.getConstant(i * 4, DL, MVT::i32);
        SDValue Ptr = DAG.getNode(ISD::ADD, DL, MVT::i32, FIN, Off);
        OutChains.push_back(
            DAG.getStore(Chain, DL, Val, Ptr, MachinePointerInfo()));
      }
    }

    FuncInfo->setVarArgsFrameIndex(VarFI);
    FuncInfo->setVarArgsSaveSize(VarArgsSaveSize);
  }

  if (!OutChains.empty())
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, OutChains);
  return Chain;
}

//===----------------------------------------------------------------------===//
// Vararg lowering
//===----------------------------------------------------------------------===//

SDValue MicroBlazeTargetLowering::LowerVASTART(SDValue Op,
                                               SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  MicroBlazeMachineFunctionInfo *FuncInfo =
      MF.getInfo<MicroBlazeMachineFunctionInfo>();
  SDLoc DL(Op);
  // va_start stores the address of the first vararg (the register save area).
  SDValue FIN = DAG.getFrameIndex(FuncInfo->getVarArgsFrameIndex(), MVT::i32);
  const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();
  return DAG.getStore(Op.getOperand(0), DL, FIN, Op.getOperand(1),
                      MachinePointerInfo(SV));
}

SDValue MicroBlazeTargetLowering::LowerVAARG(SDValue Op,
                                             SelectionDAG &DAG) const {
  SDNode *Node = Op.getNode();
  EVT VT = Node->getValueType(0);
  SDValue InChain = Node->getOperand(0);
  SDValue VAListPtr = Node->getOperand(1);
  const Value *SV = cast<SrcValueSDNode>(Node->getOperand(2))->getValue();
  SDLoc DL(Node);

  // va_list is a char* pointing at the next argument slot.
  SDValue VAList =
      DAG.getLoad(MVT::i32, DL, InChain, VAListPtr, MachinePointerInfo(SV));
  SDValue LoadChain = VAList.getValue(1);

  // Load the argument value from *ap.
  SDValue Result = DAG.getExtLoad(ISD::EXTLOAD, DL, MVT::i32, LoadChain, VAList,
                                  MachinePointerInfo(), VT);
  SDValue ResultChain = Result.getValue(1);

  // Advance ap by sizeof(type) rounded up to a 4-byte boundary (UG984 ABI).
  unsigned Bytes = ((VT.getSizeInBits() + 7) / 8 + 3) & ~3u;
  SDValue NextPtr = DAG.getNode(ISD::ADD, DL, MVT::i32, VAList,
                                DAG.getConstant(Bytes, DL, MVT::i32));
  SDValue StoreChain =
      DAG.getStore(ResultChain, DL, NextPtr, VAListPtr, MachinePointerInfo(SV));

  // Truncate back to the actual requested type if needed.
  if (VT != MVT::i32)
    Result = DAG.getNode(ISD::TRUNCATE, DL, VT, Result);

  return DAG.getMergeValues({Result, StoreChain}, DL);
}

//===----------------------------------------------------------------------===//
// Call lowering
//===----------------------------------------------------------------------===//

SDValue
MicroBlazeTargetLowering::LowerCall(TargetLowering::CallLoweringInfo &CLI,
                                    SmallVectorImpl<SDValue> &InVals) const {

  SelectionDAG &DAG = CLI.DAG;
  SDLoc &DL = CLI.DL;
  SmallVectorImpl<ISD::OutputArg> &Outs = CLI.Outs;
  SmallVectorImpl<SDValue> &OutVals = CLI.OutVals;
  SmallVectorImpl<ISD::InputArg> &Ins = CLI.Ins;
  SDValue Chain = CLI.Chain;
  SDValue Callee = CLI.Callee;
  CallingConv::ID CallConv = CLI.CallConv;
  bool IsVarArg = CLI.IsVarArg;

  // MicroBlaze does not implement tail-call optimization.  Tell the caller
  // so that the subsequent ret instruction is lowered to RTSD normally.
  CLI.IsTailCall = false;

  MachineFunction &MF = DAG.getMachineFunction();

  // Analyse outgoing arguments.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());
  CCInfo.AnalyzeCallOperands(Outs, CC_MicroBlaze);

  unsigned StackSize = CCInfo.getStackSize();
  Chain = DAG.getCALLSEQ_START(Chain, StackSize, 0, DL);

  SmallVector<std::pair<unsigned, SDValue>, 4> RegsToPass;
  SmallVector<SDValue, 12> MemOpChains;

  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    CCValAssign &VA = ArgLocs[i];
    SDValue Arg = OutVals[i];
    if (VA.isRegLoc()) {
      RegsToPass.push_back({VA.getLocReg(), Arg});
    } else {
      assert(VA.isMemLoc() && "Unexpected argument location");
      SDValue StackPtr = DAG.getRegister(MicroBlaze::R1, MVT::i32);
      SDValue PtrOff =
          DAG.getIntPtrConstant(VA.getLocMemOffset(), DL, /*IsTarget=*/false);
      SDValue MemPtr = DAG.getNode(ISD::ADD, DL, MVT::i32, StackPtr, PtrOff);
      MemOpChains.push_back(
          DAG.getStore(Chain, DL, Arg, MemPtr, MachinePointerInfo()));
    }
  }

  if (!MemOpChains.empty())
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, MemOpChains);

  // Wrap the callee if it is a GlobalAddress or ExternalSymbol.
  if (GlobalAddressSDNode *G = dyn_cast<GlobalAddressSDNode>(Callee)) {
    Callee = DAG.getTargetGlobalAddress(G->getGlobal(), DL, MVT::i32);
  } else if (ExternalSymbolSDNode *E = dyn_cast<ExternalSymbolSDNode>(Callee)) {
    Callee = DAG.getTargetExternalSymbol(E->getSymbol(), MVT::i32);
  } else {
    // Indirect call: callee is a function pointer or constant address.
    // Copy into a virtual register so BRALD r15, rA can select it.
    Register CalleeReg =
        MF.getRegInfo().createVirtualRegister(&MicroBlaze::GPRRegClass);
    Chain = DAG.getCopyToReg(Chain, DL, CalleeReg, Callee, SDValue());
    Callee = DAG.getRegister(CalleeReg, MVT::i32);
  }

  // Build the list of operands and glue.
  SDValue InFlag;
  for (auto &RTP : RegsToPass) {
    Chain = DAG.getCopyToReg(Chain, DL, RTP.first, RTP.second, InFlag);
    InFlag = Chain.getValue(1);
  }

  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);
  for (auto &RTP : RegsToPass)
    Ops.push_back(DAG.getRegister(RTP.first, MVT::i32));

  // Add a register mask for the call-clobbered registers.
  const uint32_t *Mask =
      Subtarget.getRegisterInfo()->getCallPreservedMask(MF, CallConv);
  assert(Mask && "Missing call preserved mask for calling convention");
  Ops.push_back(DAG.getRegisterMask(Mask));

  if (InFlag.getNode())
    Ops.push_back(InFlag);

  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);
  Chain = DAG.getNode(MicroBlazeISD::CALL, DL, NodeTys, Ops);
  InFlag = Chain.getValue(1);

  Chain = DAG.getCALLSEQ_END(Chain, StackSize, 0, InFlag, DL);
  InFlag = Chain.getValue(1);

  // Copy out return values.
  SmallVector<CCValAssign, 16> RVLocs;
  CCState RetCCInfo(CallConv, IsVarArg, MF, RVLocs, *DAG.getContext());
  RetCCInfo.AnalyzeCallResult(Ins, RetCC_MicroBlaze);

  for (unsigned i = 0, e = RVLocs.size(); i != e; ++i) {
    auto RVCopy = DAG.getCopyFromReg(Chain, DL, RVLocs[i].getLocReg(),
                                     RVLocs[i].getValVT(), InFlag);
    Chain = RVCopy.getValue(1);
    InFlag = RVCopy.getValue(2);
    InVals.push_back(RVCopy.getValue(0));
  }

  return Chain;
}

//===----------------------------------------------------------------------===//
// Return lowering
//===----------------------------------------------------------------------===//

SDValue MicroBlazeTargetLowering::LowerReturn(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs,
    const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
    SelectionDAG &DAG) const {

  MachineFunction &MF = DAG.getMachineFunction();

  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, *DAG.getContext());
  CCInfo.AnalyzeReturn(Outs, RetCC_MicroBlaze);

  SDValue Flag;
  SmallVector<SDValue, 4> RetOps(1, Chain);

  for (unsigned i = 0, e = RVLocs.size(); i != e; ++i) {
    CCValAssign &VA = RVLocs[i];
    Chain = DAG.getCopyToReg(Chain, DL, VA.getLocReg(), OutVals[i], Flag);
    Flag = Chain.getValue(1);
    RetOps.push_back(DAG.getRegister(VA.getLocReg(), VA.getLocVT()));
  }

  RetOps[0] = Chain;
  if (Flag.getNode())
    RetOps.push_back(Flag);

  // A true interrupt handler returns through R14 with rtid; everything else
  // (including save_volatiles) returns normally with rtsd.  Recognized via the
  // cc73 calling convention or the interrupt-handler function attribute.
  unsigned RetOpc = isMicroBlazeInterruptHandler(MF.getFunction())
                        ? MicroBlazeISD::INTR_RET
                        : MicroBlazeISD::RET_FLAG;
  return DAG.getNode(RetOpc, DL, MVT::Other, RetOps);
}

//===----------------------------------------------------------------------===//
// Inline assembly constraints
//===----------------------------------------------------------------------===//

TargetLowering::ConstraintType
MicroBlazeTargetLowering::getConstraintType(StringRef Constraint) const {
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    // 'g' = "general" (register, memory, or immediate).  MicroBlaze inline asm
    // has no complex memory addressing modes, so always satisfy 'g' with a GPR.
    // Without this override the base class maps 'g' to C_General, which on a
    // register-pressure spill falls back to memory and crashes ISel.
    case 'g':
      return C_RegisterClass;
    default:
      break;
    }
  }
  return TargetLowering::getConstraintType(Constraint);
}

std::pair<unsigned, const TargetRegisterClass *>
MicroBlazeTargetLowering::getRegForInlineAsmConstraint(
    const TargetRegisterInfo *TRI, StringRef Constraint, MVT VT) const {
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    case 'r':
      return {0U, &MicroBlaze::GPRRegClass};
    }
  }
  return TargetLowering::getRegForInlineAsmConstraint(TRI, Constraint, VT);
}

//===----------------------------------------------------------------------===//
// Carry-chain lowering (UADDO / USUBO / UADDO_CARRY / USUBO_CARRY)
//
// MicroBlaze uses MSR_C as a physical carry register.  The approach mirrors
// ARM's ARMISD::ADDC/ADDE/SUBC/SUBE: the carry flows as an MVT::i32 DAG
// value that maps to the physical MSR_C register, never passing through a GPR.
//
// valueToCarryFlag: convert boolean 0/1 carry into MSR_C flags value.
//   SUBC(value, 1): value=0 → 0-1 underflows, MSR_C=0 (no carry)
//                   value=1 → 1-1=0, MSR_C=1 (carry)
//
// carryFlagToValue: convert MSR_C flags value back to boolean 0/1.
//   ADDE(0, 0, flags): carry=0 → 0+0+0=0; carry=1 → 0+0+1=1
//
// For subtraction the carry convention is inverted:
//   LLVM USUBO_CARRY carry_in=1 means BORROW; MSR_C=1 means NO-BORROW.
//   valueToCarryFlag(borrow, Invert=true):
//     SUB(1, borrow) then SUBC(result, 1)
//     borrow=0 → 1-0=1, SUBC(1,1)=0 → MSR_C=1 (no borrow) ✓
//     borrow=1 → 1-1=0, SUBC(0,1) underflows → MSR_C=0 (borrow) ✓
//   carryFlagToValue(flags, Invert=true) = SUB(1, ADDE(0,0,flags))
//
// Round-trip elimination (PerformDAGCombine):
//   (SUBC (ADDE 0, 0, C), 1) → C
// This fires when carry-in to UADDO_CARRY is produced by carryFlagToValue
// from the preceding UADDO.  The generic DAGCombine SUB(A, SUB(A, B))→B
// handles the double-inversion in the USUBO chain.
//===----------------------------------------------------------------------===//

EVT MicroBlazeTargetLowering::getSetCCResultType(const DataLayout &DL,
                                                 LLVMContext &Context,
                                                 EVT VT) const {
  if (!VT.isVector())
    return MVT::i32;
  return VT.changeVectorElementTypeToInteger();
}

// Convert a boolean integer carry (0 or 1) into an MSR_C flags DAG value.
// If Invert=true, the input is a borrow (1=borrow), which is the opposite of
// MSR_C=1 (no-borrow), so we negate before constructing the flags.
static SDValue valueToCarryFlag(SDValue Value, SelectionDAG &DAG, bool Invert) {
  SDLoc DL(Value);
  EVT VT = Value.getValueType();
  if (Invert)
    Value = DAG.getNode(ISD::SUB, DL, MVT::i32,
                        DAG.getConstant(1, DL, MVT::i32), Value);
  // SUBC(value, 1): sets MSR_C = (value >= 1) i.e. (value != 0).
  SDValue Cmp =
      DAG.getNode(MicroBlazeISD::SUBC, DL, DAG.getVTList(VT, MVT::i32), Value,
                  DAG.getConstant(1, DL, VT));
  return Cmp.getValue(1);
}

// Convert an MSR_C flags DAG value back to a boolean integer carry (0 or 1).
// If Invert=true, the caller wants a borrow (1=borrow) rather than a carry.
static SDValue carryFlagToValue(SDValue Flags, EVT VT, SelectionDAG &DAG,
                                bool Invert) {
  SDLoc DL(Flags);
  // ADDE(0, 0, flags): 0 + 0 + MSR_C = carry (0 or 1).
  SDValue BoolCarry =
      DAG.getNode(MicroBlazeISD::ADDE, DL, DAG.getVTList(VT, MVT::i32),
                  DAG.getConstant(0, DL, VT), DAG.getConstant(0, DL, VT), Flags)
          .getValue(0);
  if (!Invert)
    return BoolCarry;
  // borrow = 1 - carry  (NOT the carry).
  return DAG.getNode(ISD::SUB, DL, VT, DAG.getConstant(1, DL, VT), BoolCarry);
}

// UADDO: unsigned add-with-overflow for i32.
//   (sum: i32, carry: i32) = UADDO(a, b)
// Lowered to MicroBlazeISD::ADDC (→ ADD machine instruction) + carry extract.
SDValue MicroBlazeTargetLowering::LowerUADDO(SDValue Op,
                                             SelectionDAG &DAG) const {
  SDLoc DL(Op);
  EVT VT = Op.getValue(0).getValueType();
  EVT CVT = Op.getValue(1).getValueType();
  SDValue Add =
      DAG.getNode(MicroBlazeISD::ADDC, DL, DAG.getVTList(VT, MVT::i32),
                  Op.getOperand(0), Op.getOperand(1));
  return DAG.getMergeValues(
      {Add, carryFlagToValue(Add.getValue(1), CVT, DAG, false)}, DL);
}

// USUBO: unsigned sub-with-overflow for i32.
//   (diff: i32, borrow: i32) = USUBO(a, b)
// Lowered to MicroBlazeISD::SUBC (→ RSUB machine instruction) + borrow extract.
SDValue MicroBlazeTargetLowering::LowerUSUBO(SDValue Op,
                                             SelectionDAG &DAG) const {
  SDLoc DL(Op);
  EVT VT = Op.getValue(0).getValueType();
  EVT CVT = Op.getValue(1).getValueType();
  // MBsubc(a, b) = RSUB(rA=b, rB=a) = a - b; MSR_C = 1 if no borrow.
  SDValue Sub =
      DAG.getNode(MicroBlazeISD::SUBC, DL, DAG.getVTList(VT, MVT::i32),
                  Op.getOperand(0), Op.getOperand(1));
  // Invert=true: extract borrow (1=borrow) from MSR_C (1=no-borrow).
  return DAG.getMergeValues(
      {Sub, carryFlagToValue(Sub.getValue(1), CVT, DAG, true)}, DL);
}

// UADDO_CARRY: carry-in unsigned add for i32.
//   (sum: i32, carry_out: i32) = UADDO_CARRY(a, b, carry_in: i32)
// carry_in=1 means carry from the lo-word add.
SDValue MicroBlazeTargetLowering::LowerUADDO_CARRY(SDValue Op,
                                                   SelectionDAG &DAG) const {
  SDLoc DL(Op);
  EVT VT = Op.getValue(0).getValueType();
  EVT CVT = Op.getValue(1).getValueType();
  SDValue CarryIn = valueToCarryFlag(Op.getOperand(2), DAG, false);
  SDValue Add =
      DAG.getNode(MicroBlazeISD::ADDE, DL, DAG.getVTList(VT, MVT::i32),
                  Op.getOperand(0), Op.getOperand(1), CarryIn);
  return DAG.getMergeValues(
      {Add, carryFlagToValue(Add.getValue(1), CVT, DAG, false)}, DL);
}

// USUBO_CARRY: borrow-in unsigned sub for i32.
//   (diff: i32, borrow_out: i32) = USUBO_CARRY(a, b, borrow_in: i32)
// borrow_in=1 means borrow from the lo-word sub (opposite of MSR_C convention).
SDValue MicroBlazeTargetLowering::LowerUSUBO_CARRY(SDValue Op,
                                                   SelectionDAG &DAG) const {
  SDLoc DL(Op);
  EVT VT = Op.getValue(0).getValueType();
  EVT CVT = Op.getValue(1).getValueType();
  // Invert=true: borrow→MSR_C inversion (borrow=0 → MSR_C=1, borrow=1 →
  // MSR_C=0).
  SDValue CarryIn = valueToCarryFlag(Op.getOperand(2), DAG, true);
  SDValue Sub =
      DAG.getNode(MicroBlazeISD::SUBE, DL, DAG.getVTList(VT, MVT::i32),
                  Op.getOperand(0), Op.getOperand(1), CarryIn);
  // Invert=true: MSR_C→borrow inversion on the way out.
  return DAG.getMergeValues(
      {Sub, carryFlagToValue(Sub.getValue(1), CVT, DAG, true)}, DL);
}

// PerformDAGCombine — carry round-trip elimination.
//
// When UADDO (lowered to MicroBlazeISD::ADDC) feeds UADDO_CARRY (lowered to
// MicroBlazeISD::ADDE), the carry round-trip looks like:
//   carryFlagToValue  : ADDE(0, 0, C)           → boolean carry
//   valueToCarryFlag  : SUBC(ADDE(0,0,C), 1)    → flags
//
// The combine fires on MicroBlazeISD::SUBC when its LHS is ADDE(0,0,C):
//   (SUBC (ADDE 0, 0, C), 1) → C   (directly return the original flags)
//
// After the combine, the carry chain collapses to:
//   MicroBlazeISD::ADDC(lo_a, lo_b)         → (lo_sum, MSR_C)
//   MicroBlazeISD::ADDE(hi_a, hi_b, MSR_C)  → (hi_sum, MSR_C)
// which selects directly to ADD + ADDC (2 instructions).
//
// For the SUBE chain, the generic DAGCombiner's A-(A-B)→B fold reduces
// the double-inversion 1-(1-x) to x before this combine fires.
SDValue
MicroBlazeTargetLowering::PerformDAGCombine(SDNode *N,
                                            DAGCombinerInfo &DCI) const {
  if (N->getOpcode() == MicroBlazeISD::SUBC && N->hasAnyUseOfValue(1)) {
    SDValue LHS = N->getOperand(0);
    SDValue RHS = N->getOperand(1);
    if (LHS->getOpcode() == MicroBlazeISD::ADDE &&
        isNullConstant(LHS->getOperand(0)) &&
        isNullConstant(LHS->getOperand(1)) && isOneConstant(RHS))
      return DCI.CombineTo(N, SDValue(N, 0), LHS->getOperand(2));
  }
  return SDValue();
}
