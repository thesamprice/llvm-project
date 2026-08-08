// Check that RTEMS targets predefine __rtems__, and that _GNU_SOURCE is
// additionally predefined in C++ mode.

// RUN: %clang_cc1 -E -dM -triple riscv32-unknown-rtems7 < /dev/null | FileCheck -check-prefix=RTEMS %s
// RUN: %clang_cc1 -E -dM -triple riscv64-unknown-rtems7 < /dev/null | FileCheck -check-prefix=RTEMS %s
// RUN: %clang_cc1 -E -dM -triple arm-unknown-rtems7 < /dev/null | FileCheck -check-prefix=RTEMS %s
// RUN: %clang_cc1 -E -dM -triple sparc-rtems-elf < /dev/null | FileCheck -check-prefix=RTEMS %s

// RTEMS: #define __rtems__ 1

// Non-RTEMS RISC-V targets must not pick up the OS macros.
// RUN: %clang_cc1 -E -dM -triple riscv64-unknown-elf < /dev/null | FileCheck -check-prefix=NORTEMS %s

// NORTEMS-NOT: __rtems__

// RUN: %clang_cc1 -x c++ -E -dM -triple riscv64-unknown-rtems7 < /dev/null | FileCheck -check-prefix=RTEMSCXX %s

// RTEMSCXX-DAG: #define _GNU_SOURCE 1
// RTEMSCXX-DAG: #define __rtems__ 1
