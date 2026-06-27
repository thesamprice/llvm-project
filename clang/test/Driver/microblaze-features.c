// RUN: %clang -target microblazeel-unknown-linux-gnu -dM -E %s | FileCheck --check-prefix=MACROS %s
// RUN: %clang -target microblazeel-unknown-elf -mxl-barrel-shift -### %s 2>&1 | FileCheck --check-prefix=BARREL %s
// RUN: %clang -target microblazeel-unknown-elf -mno-xl-barrel-shift -### %s 2>&1 | FileCheck --check-prefix=NOBARREL %s
// RUN: %clang -target microblazeel-unknown-elf -mxl-pattern-compare -### %s 2>&1 | FileCheck --check-prefix=PCMP %s
// RUN: %clang -target microblazeel-unknown-elf -mno-xl-pattern-compare -### %s 2>&1 | FileCheck --check-prefix=NOPCMP %s
// RUN: %clang -target microblazeel-unknown-elf -mno-xl-soft-div -### %s 2>&1 | FileCheck --check-prefix=HDIV %s
// RUN: %clang -target microblazeel-unknown-elf -mxl-soft-div -### %s 2>&1 | FileCheck --check-prefix=SDIV %s
// RUN: %clang -target microblazeel-unknown-elf -mxl-multiply-high -### %s 2>&1 | FileCheck --check-prefix=MULH %s
// RUN: %clang -target microblazeel-unknown-elf -mno-xl-multiply-high -### %s 2>&1 | FileCheck --check-prefix=NOMULH %s
// RUN: %clang -target microblazeel-unknown-elf -mxl-reorder -### %s 2>&1 | FileCheck --check-prefix=REORDER %s
// RUN: %clang -target microblazeel-unknown-elf -mno-xl-reorder -### %s 2>&1 | FileCheck --check-prefix=NOREORDER %s
// RUN: %clang -target microblazeel-unknown-elf -mno-xl-soft-float -### %s 2>&1 | FileCheck --check-prefix=HFPU %s
// RUN: %clang -target microblazeel-unknown-elf -mxl-soft-float -### %s 2>&1 | FileCheck --check-prefix=SFPU %s
// RUN: %clang -target microblazeel-unknown-elf -mxl-float-convert -### %s 2>&1 | FileCheck --check-prefix=FCONV %s
// RUN: %clang -target microblazeel-unknown-elf -mno-xl-float-convert -### %s 2>&1 | FileCheck --check-prefix=NOFCONV %s

// REQUIRES: microblaze-registered-target

// MACROS-DAG: #define __MICROBLAZE__ 1
// MACROS-DAG: #define __microblaze__ 1
// MACROS-DAG: #define __mb__ 1
// MACROS-DAG: #define __MICROBLAZEEL__ 1

// BARREL:   "+barrel-shift"
// NOBARREL: "-barrel-shift"
// PCMP:     "+pattern-compare"
// NOPCMP:   "-pattern-compare"
// HDIV:     "+divide"
// SDIV:     "-divide"
// MULH:     "+multiply-high"
// NOMULH:   "-multiply-high"
// REORDER:  "+reorder"
// NOREORDER: "-reorder"
// HFPU:     "+hard-float"
// SFPU:     "-hard-float"
// FCONV:    "+float-convert"
// NOFCONV:  "-float-convert"
