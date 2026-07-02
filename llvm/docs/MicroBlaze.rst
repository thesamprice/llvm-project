================
MicroBlaze Target
================

.. contents::
   :local:

Overview
========

MicroBlaze is a 32-bit soft-core RISC processor from AMD (formerly Xilinx),
implemented in FPGA fabric and configured at synthesis time via Vivado IP
parameters.  LLVM targets the little-endian bare-metal configuration with
triple ``microblazeel-unknown-elf``.

The backend is experimental: build with
``-DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=MicroBlaze``.

ISA Reference
=============

AMD UG984 — *Vivado Design Suite: MicroBlaze Processor Reference Guide*:
https://docs.amd.com/r/en-US/ug984-vivado-microblaze-ref

* Chapter 4: Application Binary Interface (register usage, stack convention,
  calling convention)
* Chapter 5: Instruction Set Architecture (instruction encodings, formats,
  opcodes, timing)

Supported Features
==================

Feature flags are set with ``-mattr=`` or the GCC-compatible ``-mxl-*``
Clang driver flags.

.. list-table::
   :header-rows: 1
   :widths: 30 25 45

   * - ``-mattr`` flag
     - Clang driver flag
     - Description
   * - ``+barrel-shift``
     - ``-mxl-barrel-shift``
     - Hardware barrel shifter (BSRL/BSLL/BSRLI/BSLLI/BSRAI/BSRAI;
       BSEFI/BSIFI bit-field extract/insert)
   * - ``+pattern-compare``
     - ``-mxl-pattern-compare``
     - Pattern-compare instructions (PCMPEQ/PCMPNE/PCMPBF; CLZ)
   * - ``+multiply-high``
     - ``-mxl-multiply-high``
     - 64-bit multiply-high instructions (MULH/MULHU/MULHSU)
   * - ``+divide``
     - (no GCC flag; enabled by Vivado ``C_USE_DIV``)
     - Hardware integer divide (IDIV/IDIVU)
   * - ``+hard-float``
     - ``-mno-xl-soft-float``
     - Hardware FPU (FADD/FSUB/FMUL/FDIV/FCMP; requires ``C_USE_FPU=1``)
   * - ``+float-convert``
     - (implied by ``+hard-float``)
     - Hardware FLT/FINT/FSQRT (``C_USE_FPU=2``)
   * - ``+reorder``
     - ``-mxl-reorder``
     - Byte/halfword reorder instructions (SWAPB/SWAPH; implies ``+swapb``,
       ``+swaph``)
   * - ``+area-optimized``
     - (no GCC flag; matches Vivado ``C_AREA_OPTIMIZED=1``)
     - Use the 3-stage area-optimized pipeline scheduling model instead of
       the default 5-stage speed model

ABI
===

* ILP32: ``int``, pointers = 32 bits; ``long long`` / ``double`` = 64 bits,
  4-byte aligned (not 8-byte).
* Arguments: R5–R10 (first 6 ``i32``-equivalent words); remainder on stack
  at ``sp+24`` and above (24-byte ABI save area).
* Return values: R3 (32-bit), R3:R4 (64-bit, R3 = high word).
* Structs passed by value are coerced to ``[N x i32]`` arrays.
* Callee-saved: R19–R31 (plus R15 = link register, saved by callees that
  make further calls).
* Stack grows downward; frame pointer is R19 when needed.

Special Calling Conventions
===========================

``__attribute__((interrupt_handler))``
  Saves/restores all volatile registers (R3–R12, R17, R18) and MSR;
  returns with ``rtid r14, 0``.  Maps to ``CallingConv::MICROBLAZE_INTR``
  (CC 73).

``__attribute__((save_volatiles))``
  Saves/restores volatile registers without MSR save; returns normally
  with ``rtsd r15, 8``.  Maps to ``CallingConv::MICROBLAZE_SVOL`` (CC 74).

Delay Slots
===========

Most branch and call instructions have a one-instruction delay slot.
LLVM fills delay slots automatically via ``MicroBlazeDelaySlotFiller``.
Non-delay variants (``brid``, ``braid``, ``bneid``, etc.) are used when
a safe filler cannot be found.

Limitations
===========

* Big-endian (``microblaze-unknown-elf``) is not supported.
* MicroBlaze-64 (MB-64) extended addressing beyond 4 GB is not supported.
* ``C_USE_MMU`` and ``C_INTERCONNECT`` TLB/cache configurations are not
  modelled; the backend targets bare-metal configurations.
