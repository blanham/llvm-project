// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -o - %s | FileCheck %s --check-prefix=LE
// RUN: %clang_cc1 -triple powerpc64-unknown-linux-gnu -emit-llvm -o - %s | FileCheck %s --check-prefix=BE

// Enum attribution test; validates that enumeration types (treated as integral)
// receive swap code when storage order mismatches target.

typedef enum E { A = 1, B = 1024 } __attribute__((scalar_storage_order("big-endian"))) beE;
typedef enum F { C1 = 7, C2 = 9 } __attribute__((scalar_storage_order("little-endian"))) leE;

beE load_enum(beE *p) {
  // LE-LABEL: define{{.*}} i32 @load_enum
  // LE: call i32 @llvm.bswap.i32
  // BE-LABEL: define{{.*}} i32 @load_enum
  // BE-NOT: call i32 @llvm.bswap.i32
  return *p;
}

void store_enum(beE *p, beE v) {
  // LE-LABEL: define{{.*}} void @store_enum
  // LE: call i32 @llvm.bswap.i32
  // BE-LABEL: define{{.*}} void @store_enum
  // BE-NOT: call i32 @llvm.bswap.i32
  *p = v;
}

leE load_enum_le(leE *p) {
  // LE-LABEL: define{{.*}} i32 @load_enum_le
  // LE-NOT: call i32 @llvm.bswap.i32
  // BE-LABEL: define{{.*}} i32 @load_enum_le
  // BE: call i32 @llvm.bswap.i32
  return *p;
}

void store_enum_le(leE *p, leE v) {
  // LE-LABEL: define{{.*}} void @store_enum_le
  // LE-NOT: call i32 @llvm.bswap.i32
  // BE-LABEL: define{{.*}} void @store_enum_le
  // BE: call i32 @llvm.bswap.i32
  *p = v;
}
