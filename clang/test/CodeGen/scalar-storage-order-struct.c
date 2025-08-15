// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -O0 -o - %s | FileCheck %s

// Big-endian and little-endian attributed typedefs.
typedef unsigned int __attribute__((scalar_storage_order("big-endian"))) be32_t;
typedef unsigned int __attribute__((scalar_storage_order("little-endian"))) le32_t;

typedef struct Mixed {
  be32_t a;     // Requires swap on load/store (target is little-endian).
  le32_t b;     // No swap (same as target).
  unsigned int plain; // No swap.
} Mixed;

be32_t test_load_a(Mixed *p) {
  // CHECK-LABEL: define{{.*}} i32 @test_load_a
  // CHECK: call i32 @llvm.bswap.i32
  return p->a;
}

le32_t test_load_b(Mixed *p) {
  // CHECK-LABEL: define{{.*}} i32 @test_load_b
  // CHECK-NOT: call i32 @llvm.bswap.i32
  return p->b;
}

unsigned test_load_plain(Mixed *p) {
  // CHECK-LABEL: define{{.*}} i32 @test_load_plain
  // CHECK-NOT: call i32 @llvm.bswap.i32
  return p->plain;
}

void test_store_a(Mixed *p, be32_t v) {
  // CHECK-LABEL: define{{.*}} void @test_store_a
  // CHECK: call i32 @llvm.bswap.i32
  p->a = v;
}

void test_store_b(Mixed *p, le32_t v) {
  // CHECK-LABEL: define{{.*}} void @test_store_b
  // CHECK-NOT: call i32 @llvm.bswap.i32
  p->b = v;
}

void test_store_plain(Mixed *p, unsigned v) {
  // CHECK-LABEL: define{{.*}} void @test_store_plain
  // CHECK-NOT: call i32 @llvm.bswap.i32
  p->plain = v;
}

be32_t load_and_return(be32_t *p) {
  // CHECK-LABEL: define{{.*}} i32 @load_and_return
  // CHECK: call i32 @llvm.bswap.i32
  return *p;
}

void store_param(be32_t *p, be32_t v) {
  // CHECK-LABEL: define{{.*}} void @store_param
  // CHECK: call i32 @llvm.bswap.i32
  *p = v;
}

le32_t load_and_return_le(le32_t *p) {
  // CHECK-LABEL: define{{.*}} i32 @load_and_return_le
  // CHECK-NOT: call i32 @llvm.bswap.i32
  return *p;
}

void store_param_le(le32_t *p, le32_t v) {
  // CHECK-LABEL: define{{.*}} void @store_param_le
  // CHECK-NOT: call i32 @llvm.bswap.i32
  *p = v;
}

// Heterogeneous copy: expect two swaps (load + store of field 'a').
void copy_mixed(Mixed *dst, Mixed *src) {
  // CHECK-LABEL: define{{.*}} void @copy_mixed
  // CHECK: call i32 @llvm.bswap.i32
  // CHECK: call i32 @llvm.bswap.i32
  dst->a = src->a; // swap on load, swap on store
  dst->b = src->b; // no swaps
  dst->plain = src->plain; // no swaps
}
