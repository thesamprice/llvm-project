// RUN: %clang_cc1 -triple microblazeel-unknown-elf -emit-llvm -o - %s | FileCheck %s
// REQUIRES: microblaze-registered-target

// __attribute__((interrupt_handler)) / __attribute__((save_volatiles)) mark a
// function so the MicroBlaze backend gives it the interrupt-style prologue and
// epilogue (preserve the volatile R3-R12 + R17/R18; an interrupt handler also
// saves MSR and returns with rtid).  These are emitted as IR *function
// attributes* — "interrupt-handler" / "save-volatiles" — rather than a non-
// default calling convention, so ordinary call sites stay ABI-compatible.

// CHECK: define{{.*}} void @isr() [[ISR:#[0-9]+]]
__attribute__((interrupt_handler)) void isr(void) {}

// CHECK: define{{.*}} void @svol() [[SVOL:#[0-9]+]]
__attribute__((save_volatiles)) void svol(void) {}

// A plain function gets neither attribute.
// CHECK: define{{.*}} void @plain() [[PLAIN:#[0-9]+]]
void plain(void) {}

// Handlers are also kept noinline (they must own their frame).
// CHECK-DAG: attributes [[ISR]] = {{.*}}noinline{{.*}}"interrupt-handler"
// CHECK-DAG: attributes [[SVOL]] = {{.*}}noinline{{.*}}"save-volatiles"

// The plain function carries neither marker.
// CHECK-DAG: attributes [[PLAIN]] =
// CHECK-NOT: attributes [[PLAIN]] = {{.*}}"interrupt-handler"
// CHECK-NOT: attributes [[PLAIN]] = {{.*}}"save-volatiles"
