================
MicroBlaze Support
================

.. contents::
   :local:

Overview
========

Clang supports cross-compilation for the AMD MicroBlaze 32-bit soft-core
processor targeting ``microblazeel-unknown-elf`` (little-endian bare-metal).

The backend is experimental.  Build LLVM with::

  -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=MicroBlaze

Example cross-compilation invocation::

  clang --target=microblazeel-unknown-elf -mxl-barrel-shift -O2 hello.c \
        -nostdlib -T linker.ld -o hello.elf

Feature Flags
=============

MicroBlaze IP cores are configured at synthesis time; the Clang flags mirror
the Vivado IP parameters so compiler-generated code matches the hardware.

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Flag
     - Description
   * - ``-mxl-barrel-shift`` / ``-mno-xl-barrel-shift``
     - Enable/disable hardware barrel shifter (``C_USE_BARREL_SHIFTER``)
   * - ``-mxl-pattern-compare`` / ``-mno-xl-pattern-compare``
     - Enable/disable pattern-compare and CLZ instructions (``C_USE_PCMP_INSTR``)
   * - ``-mxl-multiply-high`` / ``-mno-xl-multiply-high``
     - Enable/disable 64-bit multiply-high instructions (``C_USE_HW_MUL=2``)
   * - ``-mno-xl-soft-float`` / ``-mxl-soft-float``
     - Enable/disable hardware FPU (``C_USE_FPU=1``); default is software
       floating-point via compiler-rt libcalls
   * - ``-mxl-float-convert`` / ``-mno-xl-float-convert``
     - Enable/disable hardware float/int conversion instructions (flt, fint,
       fsqrt; ``C_USE_FPU=2``); combine with ``-mno-xl-soft-float``
   * - ``-mxl-reorder`` / ``-mno-xl-reorder``
     - Enable/disable byte/halfword reorder instructions (``C_USE_REORDER_INSTR``)
   * - ``-mno-xl-soft-div``
     - Enable hardware integer divide (``C_USE_DIV``); default is software

Special Attributes
==================

``__attribute__((interrupt_handler))``
  Generates an interrupt service routine prologue/epilogue: saves volatile
  registers R3–R12, R17, R18 and MSR; returns with ``rtid r14, 0``.

``__attribute__((save_volatiles))``
  Saves/restores volatile registers without MSR; returns normally.

See :doc:`../llvm/MicroBlaze` for the LLVM backend reference documentation.
