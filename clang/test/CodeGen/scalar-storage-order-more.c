// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -o - %s | FileCheck %s --check-prefix=LE
// RUN: %clang_cc1 -triple powerpc64-unknown-linux-gnu -emit-llvm -o - %s | FileCheck %s --check-prefix=BE

// Big-endian storage on a little-endian target should introduce bswap.
typedef unsigned short __attribute__((scalar_storage_order("big-endian"))) be16_t;
typedef unsigned int   __attribute__((scalar_storage_order("big-endian"))) be32_t;
typedef unsigned long long __attribute__((scalar_storage_order("big-endian"))) be64_t;

typedef float  __attribute__((scalar_storage_order("big-endian"))) befloat;
typedef double __attribute__((scalar_storage_order("big-endian"))) bedouble;

typedef unsigned char __attribute__((scalar_storage_order("big-endian"))) be8_t; // width 8 -> no swap

// Little-endian storage (matches x86_64) should not introduce bswap on LE but will on BE.
typedef unsigned int __attribute__((scalar_storage_order("little-endian"))) le32_t;
typedef unsigned short __attribute__((scalar_storage_order("little-endian"))) le16_t;
typedef unsigned long long __attribute__((scalar_storage_order("little-endian"))) le64_t;
typedef float __attribute__((scalar_storage_order("little-endian"))) lefloat;
typedef double __attribute__((scalar_storage_order("little-endian"))) ledouble;
typedef unsigned char __attribute__((scalar_storage_order("little-endian"))) le8_t; // width 8 -> no swap

be16_t load16(be16_t *p) { // LE-LABEL: @load16
  // LE: call i16 @llvm.bswap.i16
  // BE-NOT: call i16 @llvm.bswap.i16
  return *p; // BE-LABEL: @load16
}

be32_t load32(be32_t *p) { // LE-LABEL: @load32
  // LE: call i32 @llvm.bswap.i32
  // BE-NOT: call i32 @llvm.bswap.i32
  return *p; // BE-LABEL: @load32
}

be64_t load64(be64_t *p) { // LE-LABEL: @load64
  // LE: call i64 @llvm.bswap.i64
  // BE-NOT: call i64 @llvm.bswap.i64
  return *p; // BE-LABEL: @load64
}

befloat loadF(befloat *p) { // LE-LABEL: @loadF
  // LE: call i32 @llvm.bswap.i32
  // BE-NOT: call i32 @llvm.bswap.i32
  return *p; // BE-LABEL: @loadF
}

bedouble loadD(bedouble *p) { // LE-LABEL: @loadD
  // LE: call i64 @llvm.bswap.i64
  // BE-NOT: call i64 @llvm.bswap.i64
  return *p; // BE-LABEL: @loadD
}

be8_t load8(be8_t *p) { // LE-LABEL: @load8
  // LE-NOT: call i8 @llvm.bswap
  // BE-NOT: call i8 @llvm.bswap
  return *p; // BE-LABEL: @load8
}

void store32(be32_t *p, be32_t v) { // LE-LABEL: @store32
  // LE: call i32 @llvm.bswap.i32
  // BE-NOT: call i32 @llvm.bswap.i32
  *p = v; // BE-LABEL: @store32
}

le32_t load_le32(le32_t *p) { // LE-LABEL: @load_le32
  // LE-NOT: call i32 @llvm.bswap.i32
  // BE: call i32 @llvm.bswap.i32
  return *p; // BE-LABEL: @load_le32
}

void store_le32(le32_t *p, le32_t v) { // LE-LABEL: @store_le32
  // LE-NOT: call i32 @llvm.bswap.i32
  // BE: call i32 @llvm.bswap.i32
  *p = v; // BE-LABEL: @store_le32
}

// Additional little-endian storage types (mismatch on BE target) to complete matrix.

le16_t load_le16(le16_t *p) { // LE-LABEL: @load_le16
  // LE-NOT: call i16 @llvm.bswap.i16
  // BE: call i16 @llvm.bswap.i16
  return *p; // BE-LABEL: @load_le16
}

void store_le16(le16_t *p, le16_t v) { // LE-LABEL: @store_le16
  // LE-NOT: call i16 @llvm.bswap.i16
  // BE: call i16 @llvm.bswap.i16
  *p = v; // BE-LABEL: @store_le16
}

le64_t load_le64(le64_t *p) { // LE-LABEL: @load_le64
  // LE-NOT: call i64 @llvm.bswap.i64
  // BE: call i64 @llvm.bswap.i64
  return *p; // BE-LABEL: @load_le64
}

void store_le64(le64_t *p, le64_t v) { // LE-LABEL: @store_le64
  // LE-NOT: call i64 @llvm.bswap.i64
  // BE: call i64 @llvm.bswap.i64
  *p = v; // BE-LABEL: @store_le64
}

lefloat load_leF(lefloat *p) { // LE-LABEL: @load_leF
  // LE-NOT: call i32 @llvm.bswap.i32
  // BE: call i32 @llvm.bswap.i32
  return *p; // BE-LABEL: @load_leF
}

void store_leF(lefloat *p, lefloat v) { // LE-LABEL: @store_leF
  // LE-NOT: call i32 @llvm.bswap.i32
  // BE: call i32 @llvm.bswap.i32
  *p = v; // BE-LABEL: @store_leF
}

ledouble load_leD(ledouble *p) { // LE-LABEL: @load_leD
  // LE-NOT: call i64 @llvm.bswap.i64
  // BE: call i64 @llvm.bswap.i64
  return *p; // BE-LABEL: @load_leD
}

void store_leD(ledouble *p, ledouble v) { // LE-LABEL: @store_leD
  // LE-NOT: call i64 @llvm.bswap.i64
  // BE: call i64 @llvm.bswap.i64
  *p = v; // BE-LABEL: @store_leD
}

le8_t load_le8(le8_t *p) { // LE-LABEL: @load_le8
  // LE-NOT: call i8 @llvm.bswap
  // BE-NOT: call i8 @llvm.bswap
  return *p; // BE-LABEL: @load_le8
}

void store_le8(le8_t *p, le8_t v) { // LE-LABEL: @store_le8
  // LE-NOT: call i8 @llvm.bswap
  // BE-NOT: call i8 @llvm.bswap
  *p = v; // BE-LABEL: @store_le8
}
