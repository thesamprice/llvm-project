/// Check the RTEMS ToolChain.
///
/// RTEMS is a statically linked single address space RTOS.  Before there was a
/// ToolChain an RTEMS triple fell through to a generic path which handed the
/// link to whatever "gcc" or "ld" was on PATH, and everything the GCC -qrtems
/// spec supplies -- the start files, the linker command file and the group
/// around the mutually dependent RTEMS libraries -- had to be passed by hand.

// RUN: %clang -### --target=riscv32-unknown-rtems7 -mabi=ilp32f \
// RUN:   --sysroot=%S/Inputs/basic_rtems_tree/riscv-rtems7 \
// RUN:   --gcc-install-dir=%S/Inputs/basic_rtems_tree/lib/gcc/riscv-rtems7/15.2.0 \
// RUN:   %s 2>&1 | FileCheck %s

/// The BSP supplies the linker command file.  Without it a link can succeed
/// and still produce an unbootable image, which is a worse failure than an
/// error, so it is passed by default.
// CHECK: "-Tlinkcmds"

/// The start files come from the multilib matching -mabi, not from the top of
/// the GCC installation; picking the wrong one links objects built for a
/// different floating point ABI.
// CHECK-SAME: "{{.*}}rv32imafc/ilp32f{{/|\\\\}}crti.o"
// CHECK-SAME: "{{.*}}rv32imafc/ilp32f{{/|\\\\}}crtbegin.o"

/// The RTEMS libraries are mutually dependent, so a linker which makes one
/// pass over each archive cannot resolve them in any fixed order.
// CHECK-SAME: "--start-group" "-lrtemsbsp" "-lrtemscpu" "-latomic" "-lc" "-lgcc" "--end-group"

// CHECK-SAME: "{{.*}}rv32imafc/ilp32f{{/|\\\\}}crtend.o"
// CHECK-SAME: "{{.*}}rv32imafc/ilp32f{{/|\\\\}}crtn.o"

/// -nostdlib drops the libraries and the linker script but is not an error.
// RUN: %clang -### --target=riscv32-unknown-rtems7 -mabi=ilp32f -nostdlib \
// RUN:   --sysroot=%S/Inputs/basic_rtems_tree/riscv-rtems7 \
// RUN:   --gcc-install-dir=%S/Inputs/basic_rtems_tree/lib/gcc/riscv-rtems7/15.2.0 \
// RUN:   %s 2>&1 | FileCheck --check-prefix=NOSTDLIB %s

// NOSTDLIB-NOT: "-Tlinkcmds"
// NOSTDLIB-NOT: "--start-group"

/// __rtems__ is predefined for RISC-V, which it was not before the target info
/// gained a Triple::RTEMS case for riscv32/riscv64.
// RUN: %clang --target=riscv32-unknown-rtems7 -dM -E %s | FileCheck --check-prefix=DEFINES %s
// DEFINES: #define __rtems__ 1

int main(void) { return 0; }
