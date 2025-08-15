// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -O2 -emit-llvm -o - %s | FileCheck %s

typedef unsigned int __attribute__((scalar_storage_order("big-endian"))) be32_t;
typedef unsigned short __attribute__((scalar_storage_order("big-endian"))) be16_t;

be32_t load32(be32_t *p) { return *p; }
// CHECK-LABEL: define{{.*}} i32 @load32
// CHECK: call i32 @llvm.bswap.i32

void store32(be32_t *p, be32_t v) { *p = v; }
// CHECK-LABEL: define{{.*}} void @store32
// CHECK: call i32 @llvm.bswap.i32

be16_t load16(be16_t *p) { return *p; }
// CHECK-LABEL: define{{.*}} i16 @load16
// CHECK: call i16 @llvm.bswap.i16

void copy_one(be32_t *dst, be32_t *src) { *dst = *src; }
// CHECK-LABEL: define{{.*}} void @copy_one
// CHECK: call i32 @llvm.bswap.i32
// CHECK: call i32 @llvm.bswap.i32

static inline be32_t id(be32_t x) { return x; }
be32_t inline_path(be32_t *p) { return id(*p); }
// CHECK-LABEL: define{{.*}} i32 @inline_path
// CHECK: call i32 @llvm.bswap.i32

// Ensure no extra swap when value is materialized once then reused locally.
be32_t load_then_add(be32_t *p) {
  be32_t t = *p; // single load + swap
  return t + t;  // arithmetic in native order, no additional bswap
}
// CHECK-LABEL: define{{.*}} i32 @load_then_add
// CHECK: call i32 @llvm.bswap.i32
// CHECK-NOT: call i32 @llvm.bswap.i32
