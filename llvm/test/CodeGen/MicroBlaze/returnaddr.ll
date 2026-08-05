; RUN: llc -mtriple=microblazeel -verify-machineinstrs < %s | FileCheck %s

; __builtin_return_address(0) → copy R15 (link register) at function entry.
; CHECK-LABEL: ret_addr_depth0:
; CHECK: addk r3, r15, r0
define ptr @ret_addr_depth0() nounwind {
  %ra = call ptr @llvm.returnaddress(i32 0)
  ret ptr %ra
}

; __builtin_return_address(depth>0) → null (MicroBlaze has no saved FP chain).
; CHECK-LABEL: ret_addr_depth1:
; CHECK: addik r3, r0, 0
define ptr @ret_addr_depth1() nounwind {
  %ra = call ptr @llvm.returnaddress(i32 1)
  ret ptr %ra
}

; CHECK-LABEL: ret_addr_depth5:
; CHECK: addik r3, r0, 0
define ptr @ret_addr_depth5() nounwind {
  %ra = call ptr @llvm.returnaddress(i32 5)
  ret ptr %ra
}

declare ptr @llvm.returnaddress(i32 immarg)
