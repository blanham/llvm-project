// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -O0 -o - %s | FileCheck %s --check-prefix=LE
// RUN: %clang_cc1 -triple powerpc64-unknown-linux-gnu -emit-llvm -O0 -o - %s | FileCheck %s --check-prefix=BE

// 128-bit integer big-endian storage.
typedef unsigned __int128 __attribute__((scalar_storage_order("big-endian"))) be128_t;

be128_t load128(be128_t *p) {
  // LE-LABEL: define{{.*}} i128 @load128
  // LE: call i128 @llvm.bswap.i128
  // BE-LABEL: define{{.*}} i128 @load128
  // BE-NOT: call i128 @llvm.bswap.i128
  return *p;
}

void store128(be128_t *p, be128_t v) {
  // LE-LABEL: define{{.*}} void @store128
  // LE: call i128 @llvm.bswap.i128
  // BE-LABEL: define{{.*}} void @store128
  // BE-NOT: call i128 @llvm.bswap.i128
  *p = v;
}

// 8-bit integer: no swap expected (single byte).
typedef unsigned char __attribute__((scalar_storage_order("big-endian"))) be8_t;
be8_t load8(be8_t *p) {
  // LE-LABEL: define{{.*}} i8 @load8
  // LE-NOT: call i8 @llvm.bswap
  // BE-LABEL: define{{.*}} i8 @load8
  // BE-NOT: call i8 @llvm.bswap
  return *p;
}
void store8(be8_t *p, be8_t v) {
  // LE-LABEL: define{{.*}} void @store8
  // LE-NOT: call i8 @llvm.bswap
  // BE-LABEL: define{{.*}} void @store8
  // BE-NOT: call i8 @llvm.bswap
  *p = v;
}

// _Float16 (if supported) should get a 16-bit swap. Use _Float16 (C11+ extension) which maps to half.
typedef _Float16 __attribute__((scalar_storage_order("big-endian"))) behf16;
behf16 loadh(behf16 *p) {
  // LE-LABEL: define{{.*}} half @loadh
  // LE: call i16 @llvm.bswap.i16
  // BE-LABEL: define{{.*}} half @loadh
  // BE-NOT: call i16 @llvm.bswap.i16
  return *p;
}
void storeh(behf16 *p, behf16 v) {
  // LE-LABEL: define{{.*}} void @storeh
  // LE: call i16 @llvm.bswap.i16
  // BE-LABEL: define{{.*}} void @storeh
  // BE-NOT: call i16 @llvm.bswap.i16
  *p = v;
}

// long double on x86_64 is typically 80-bit; current implementation does not swap 80-bit FP.
typedef long double __attribute__((scalar_storage_order("big-endian"))) beldbl_t;
beldbl_t loadld(beldbl_t *p) {
  // LE-LABEL: define{{.*}} x86_fp80 @loadld
  // LE-NOT: call i80 @llvm.bswap
  // LE-NOT: call i128 @llvm.bswap
  // BE-LABEL: define{{.*}} x86_fp80 @loadld
  // BE-NOT: call i80 @llvm.bswap
  return *p;
}
void storeld(beldbl_t *p, beldbl_t v) {
  // LE-LABEL: define{{.*}} void @storeld
  // LE-NOT: call i80 @llvm.bswap
  // LE-NOT: call i128 @llvm.bswap
  // BE-LABEL: define{{.*}} void @storeld
  // BE-NOT: call i80 @llvm.bswap
  *p = v;
}

// Little-endian attributed types (mismatch swap expected on big-endian target; no swap on little-endian target)
typedef unsigned __int128 __attribute__((scalar_storage_order("little-endian"))) le128_t;
le128_t load_le128(le128_t *p) {
  // LE-LABEL: define{{.*}} i128 @load_le128
  // LE-NOT: call i128 @llvm.bswap.i128
  // BE-LABEL: define{{.*}} i128 @load_le128
  // BE: call i128 @llvm.bswap.i128
  return *p;
}
void store_le128(le128_t *p, le128_t v) {
  // LE-LABEL: define{{.*}} void @store_le128
  // LE-NOT: call i128 @llvm.bswap.i128
  // BE-LABEL: define{{.*}} void @store_le128
  // BE: call i128 @llvm.bswap.i128
  *p = v;
}

typedef _Float16 __attribute__((scalar_storage_order("little-endian"))) lehf16;
lehf16 load_leh(lehf16 *p) {
  // LE-LABEL: define{{.*}} half @load_leh
  // LE-NOT: call i16 @llvm.bswap.i16
  // BE-LABEL: define{{.*}} half @load_leh
  // BE: call i16 @llvm.bswap.i16
  return *p;
}
void store_leh(lehf16 *p, lehf16 v) {
  // LE-LABEL: define{{.*}} void @store_leh
  // LE-NOT: call i16 @llvm.bswap.i16
  // BE-LABEL: define{{.*}} void @store_leh
  // BE: call i16 @llvm.bswap.i16
  *p = v;
}
