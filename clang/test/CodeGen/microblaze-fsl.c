// RUN: %clang_cc1 -triple microblazeel-unknown-elf -target-feature +fsl \
// RUN:   -O0 -emit-llvm -o - %s | FileCheck %s
// REQUIRES: microblaze-registered-target

// The __builtin_microblaze_fsl_* builtins lower to the matching
// llvm.microblaze.fsl.* intrinsics (FSL/AXI-Stream, UG984 §5).

// CHECK-LABEL: define {{.*}} @read(
int read(int port) {
  // CHECK: call i32 @llvm.microblaze.fsl.get(i32 3)
  int a = __builtin_microblaze_fsl_get(3);
  // CHECK: call i32 @llvm.microblaze.fsl.tnecaget(i32 %{{.*}})
  int b = __builtin_microblaze_fsl_tnecaget(port);
  // CHECK: call i32 @llvm.microblaze.fsl.nget(i32 %{{.*}})
  int c = __builtin_microblaze_fsl_nget(port);
  return a + b + c;
}

// CHECK-LABEL: define {{.*}} @write(
void write(int value, int port) {
  // CHECK: call void @llvm.microblaze.fsl.put(i32 %{{.*}}, i32 7)
  __builtin_microblaze_fsl_put(value, 7);
  // CHECK: call void @llvm.microblaze.fsl.ncput(i32 %{{.*}}, i32 %{{.*}})
  __builtin_microblaze_fsl_ncput(value, port);
}
