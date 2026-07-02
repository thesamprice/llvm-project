// RUN: %clang_cc1 -triple microblazeel-unknown-elf -emit-llvm -o - %s \
// RUN:   | FileCheck %s
// REQUIRES: microblaze-registered-target

// Verifies the MicroBlaze ILP32 ABI lowering in clang/lib/CodeGen/Targets/MicroBlaze.cpp:
//  - Scalars passed/returned directly
//  - Aggregates (structs) coerced to [N x i32] per UG984 Ch.4 §ABI
//  - Small struct return (≤8 bytes) in [N x i32]
//  - Large struct return via hidden sret pointer
//  - i64 in R3:R4 (lower half in R3 for LE)
//  - char/short promoted to i32 on return

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long long u64;

// Scalars ---------------------------------------------------------------

// CHECK-LABEL: define {{.*}} i32 @ret_i32()
u32 ret_i32(void) { return 42; }

// CHECK-LABEL: define {{.*}} i64 @ret_i64()
u64 ret_i64(void) { return 0; }

// Promotable return: char and short get zeroext attribute (not promoted to i32 in IR type).
// CHECK-LABEL: define {{.*}} zeroext i8 @ret_u8(
u8 ret_u8(void) { return 0; }

// CHECK-LABEL: define {{.*}} zeroext i16 @ret_u16(
u16 ret_u16(void) { return 0; }

// Struct argument and return (small) ------------------------------------

struct S4 { u32 a; };
// 4-byte struct: coerced to [1 x i32] in both arg and return position.
// CHECK-LABEL: define {{.*}} @pass_s4([1 x i32] %
void pass_s4(struct S4 s) { (void)s.a; }

// CHECK-LABEL: define {{.*}} [1 x i32] @ret_s4(
struct S4 ret_s4(void) { struct S4 s = {1}; return s; }

struct S8 { u32 a; u32 b; };
// 8-byte struct: coerced to [2 x i32].
// CHECK-LABEL: define {{.*}} @pass_s8([2 x i32] %
void pass_s8(struct S8 s) { (void)(s.a + s.b); }

// CHECK-LABEL: define {{.*}} [2 x i32] @ret_s8(
struct S8 ret_s8(void) { struct S8 s = {1, 2}; return s; }

// Large struct return (>8 bytes) — hidden sret pointer -------------------

struct S12 { u32 a; u32 b; u32 c; };
// CHECK-LABEL: define {{.*}} @ret_s12(ptr {{.*}} sret
struct S12 ret_s12(void) { struct S12 s = {1, 2, 3}; return s; }

// Struct with a mix of sizes (6 bytes → 2 words = [2 x i32]) ------------

struct S6 { u16 x; u32 y; };
// 6-byte struct: ceilingdiv(6, 4) = 2 words → [2 x i32].
// CHECK-LABEL: define {{.*}} @pass_s6([2 x i32] %
void pass_s6(struct S6 s) { (void)(s.x + s.y); }

// CHECK-LABEL: define {{.*}} [2 x i32] @ret_s6(
struct S6 ret_s6(void) { struct S6 s = {1, 2}; return s; }
