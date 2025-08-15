// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -o - %s | FileCheck %s

// Enum attribution test; validates that enumeration types (treated as integral)
// receive swap code when storage order mismatches target.

typedef enum E { A = 1, B = 1024 } __attribute__((scalar_storage_order("big-endian"))) beE;

beE load_enum(beE *p) {
  // CHECK-LABEL: define{{.*}} i32 @load_enum
  // CHECK: call i32 @llvm.bswap.i32
  return *p;
}

void store_enum(beE *p, beE v) {
  // CHECK-LABEL: define{{.*}} void @store_enum
  // CHECK: call i32 @llvm.bswap.i32
  *p = v;
}
