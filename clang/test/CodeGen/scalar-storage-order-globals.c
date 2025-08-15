// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -o - %s | FileCheck %s

typedef unsigned int __attribute__((scalar_storage_order("big-endian"))) be32_t;

// Constant initializer: stored in big-endian byte order; no runtime bswap in a ctor.
be32_t g_const = 0x01020304u; // CHECK: @g_const = global i32 0x04030201

// Non-constant pointer to force a load path.
extern be32_t *gp;

be32_t read_const() {
  // CHECK-LABEL: define{{.*}} i32 @read_const
  // CHECK: load i32, ptr @g_const
  // CHECK: call i32 @llvm.bswap.i32
  return g_const;
}

// Load through external pointer (still swap).
be32_t read_ptr() {
  // CHECK-LABEL: define{{.*}} i32 @read_ptr
  // CHECK: load i32, ptr @gp
  // CHECK: load i32
  // CHECK: call i32 @llvm.bswap.i32
  return *gp;
}
