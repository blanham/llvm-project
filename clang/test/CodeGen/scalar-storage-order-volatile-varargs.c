// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -O0 -o - %s | FileCheck %s

#include <stdarg.h>

typedef unsigned int __attribute__((scalar_storage_order("big-endian"))) be32_t;

typedef unsigned short __attribute__((scalar_storage_order("big-endian"))) be16_t;

// Volatile global pointer to attributed type
extern volatile be32_t *gptr32;
extern volatile be16_t *gptr16;

be32_t vol_load32() {
  // CHECK-LABEL: define{{.*}} i32 @vol_load32
  // CHECK: call i32 @llvm.bswap.i32
  return *gptr32;
}

void vol_store32(be32_t v) {
  // CHECK-LABEL: define{{.*}} void @vol_store32
  // CHECK: call i32 @llvm.bswap.i32
  *gptr32 = v;
}

be16_t vol_load16() {
  // CHECK-LABEL: define{{.*}} i16 @vol_load16
  // CHECK: call i16 @llvm.bswap.i16
  return *gptr16;
}

void vol_store16(be16_t v) {
  // CHECK-LABEL: define{{.*}} void @vol_store16
  // CHECK: call i16 @llvm.bswap.i16
  *gptr16 = v;
}

// Variadic sink: we expect no bswap inside since values are already in native order when passed.
static unsigned accum;
void sink(int count, ...) {
  // CHECK-LABEL: define{{.*}} void @sink
  // CHECK-NOT: call i32 @llvm.bswap.i32
  va_list ap; va_start(ap, count);
  for (int i=0;i<count;++i)
    accum += va_arg(ap, unsigned); // retrieve as plain unsigned
  va_end(ap);
}

// Caller performing the swaps when passing big-endian attributed arguments.
void test_varargs(be32_t a, be32_t b) {
  // CHECK-LABEL: define{{.*}} void @test_varargs
  // Expect exactly two bswaps: one load (if from memory) each when passing 'a' and 'b'.
  // Since parameters are already attributed scalars in registers/allocas, taking their value triggers a load with swap per argument.
  // CHECK: call i32 @llvm.bswap.i32
  // CHECK: call i32 @llvm.bswap.i32
  // CHECK-NOT: call i32 @llvm.bswap.i32
  sink(2, a, b);
}
