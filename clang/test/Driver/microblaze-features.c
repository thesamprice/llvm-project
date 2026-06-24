// RUN: %clang -target microblazeel-unknown-linux-gnu -dM -E %s | FileCheck %s

// CHECK-DAG: #define __MICROBLAZE__ 1
// CHECK-DAG: #define __microblaze__ 1
// CHECK-DAG: #define __mb__ 1
// CHECK-DAG: #define __MICROBLAZEEL__ 1

// REQUIRES: microblaze-registered-target
