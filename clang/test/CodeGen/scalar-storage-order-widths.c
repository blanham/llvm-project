// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -O0 -o - %s | FileCheck %s

// 128-bit integer big-endian storage.
typedef unsigned __int128 __attribute__((scalar_storage_order("big-endian"))) be128_t;

be128_t load128(be128_t *p) {
  // CHECK-LABEL: define{{.*}} i128 @load128
  // CHECK: load i128, ptr
  // CHECK: call i128 @llvm.bswap.i128
  return *p;
}

void store128(be128_t *p, be128_t v) {
  // CHECK-LABEL: define{{.*}} void @store128
  // CHECK: call i128 @llvm.bswap.i128
  // CHECK: store i128
  *p = v;
}

// 8-bit integer: no swap expected (single byte).
typedef unsigned char __attribute__((scalar_storage_order("big-endian"))) be8_t;
be8_t load8(be8_t *p) {
  // CHECK-LABEL: define{{.*}} i8 @load8
  // CHECK-NOT: call i8 @llvm.bswap
  return *p;
}
void store8(be8_t *p, be8_t v) {
  // CHECK-LABEL: define{{.*}} void @store8
  // CHECK-NOT: call i8 @llvm.bswap
  *p = v;
}

// _Float16 (if supported) should get a 16-bit swap. Use _Float16 (C11+ extension) which maps to half.
typedef _Float16 __attribute__((scalar_storage_order("big-endian"))) behf16;
behf16 loadh(behf16 *p) {
  // CHECK-LABEL: define{{.*}} half @loadh
  // CHECK: call i16 @llvm.bswap.i16
  return *p;
}
void storeh(behf16 *p, behf16 v) {
  // CHECK-LABEL: define{{.*}} void @storeh
  // CHECK: call i16 @llvm.bswap.i16
  *p = v;
}

// long double on x86_64 is typically 80-bit; current implementation does not swap 80-bit FP.
typedef long double __attribute__((scalar_storage_order("big-endian"))) beldbl_t;
beldbl_t loadld(beldbl_t *p) {
  // CHECK-LABEL: define{{.*}} x86_fp80 @loadld
  // CHECK-NOT: call i80 @llvm.bswap
  // CHECK-NOT: call i128 @llvm.bswap
  return *p;
}
void storeld(beldbl_t *p, beldbl_t v) {
  // CHECK-LABEL: define{{.*}} void @storeld
  // CHECK-NOT: call i80 @llvm.bswap
  // CHECK-NOT: call i128 @llvm.bswap
  *p = v;
}
