// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -o - %s | FileCheck %s --check-prefix=LITTLE
// RUN: %clang_cc1 -triple powerpc64-unknown-linux-gnu -emit-llvm -o - %s | FileCheck %s --check-prefix=BIG

// Test scalar_storage_order attribute CodeGen on little-endian target (x86_64)

struct __attribute__((scalar_storage_order("big-endian"))) BigEndianStruct {
  int x;
  short y;
  long long z;
};

struct __attribute__((scalar_storage_order("little-endian"))) LittleEndianStruct {
  int x;
  short y;
  long long z;
};

// Test load from big-endian struct on little-endian target (should byte-swap)
// LITTLE-LABEL: define {{.*}} @test_load_big_endian
// LITTLE: load i32, ptr
// LITTLE-NEXT: call i32 @llvm.bswap.i32(i32
// LITTLE: load i16, ptr
// LITTLE-NEXT: call i16 @llvm.bswap.i16(i16
// LITTLE: load i64, ptr
// LITTLE-NEXT: call i64 @llvm.bswap.i64(i64

// BIG-LABEL: define {{.*}} @test_load_big_endian
// BIG: load i32, ptr
// BIG-NOT: call i32 @llvm.bswap.i32
// BIG: load i16, ptr
// BIG-NOT: call i16 @llvm.bswap.i16
// BIG: load i64, ptr
// BIG-NOT: call i64 @llvm.bswap.i64

void test_load_big_endian(struct BigEndianStruct *s, int *a, short *b, long long *c) {
  *a = s->x;
  *b = s->y;
  *c = s->z;
}

// Test store to big-endian struct on little-endian target (should byte-swap)
// LITTLE-LABEL: define {{.*}} @test_store_big_endian
// LITTLE: call i32 @llvm.bswap.i32(i32
// LITTLE-NEXT: store i32
// LITTLE: call i16 @llvm.bswap.i16(i16
// LITTLE-NEXT: store i16
// LITTLE: call i64 @llvm.bswap.i64(i64
// LITTLE-NEXT: store i64

// BIG-LABEL: define {{.*}} @test_store_big_endian
// BIG: store i32
// BIG-NOT: call i32 @llvm.bswap.i32
// BIG: store i16
// BIG-NOT: call i16 @llvm.bswap.i16
// BIG: store i64
// BIG-NOT: call i64 @llvm.bswap.i64

void test_store_big_endian(struct BigEndianStruct *s, int a, short b, long long c) {
  s->x = a;
  s->y = b;
  s->z = c;
}

// Test load from little-endian struct on little-endian target (no byte-swap)
// LITTLE-LABEL: define {{.*}} @test_load_little_endian
// LITTLE: load i32, ptr
// LITTLE-NOT: call i32 @llvm.bswap.i32
// LITTLE: load i16, ptr
// LITTLE-NOT: call i16 @llvm.bswap.i16
// LITTLE: load i64, ptr
// LITTLE-NOT: call i64 @llvm.bswap.i64

// BIG-LABEL: define {{.*}} @test_load_little_endian
// BIG: load i32, ptr
// BIG-NEXT: call i32 @llvm.bswap.i32
// BIG: load i16, ptr
// BIG-NEXT: call i16 @llvm.bswap.i16
// BIG: load i64, ptr
// BIG-NEXT: call i64 @llvm.bswap.i64

void test_load_little_endian(struct LittleEndianStruct *s, int *a, short *b, long long *c) {
  *a = s->x;
  *b = s->y;
  *c = s->z;
}

// Test array fields
struct __attribute__((scalar_storage_order("big-endian"))) ArrayStruct {
  int arr[4];
  short sarr[8];
};

// Test array element access (should byte-swap)
// LITTLE-LABEL: define {{.*}} @test_array_access
// LITTLE: load i32, ptr
// LITTLE-NEXT: call i32 @llvm.bswap.i32(i32
// LITTLE: load i16, ptr
// LITTLE-NEXT: call i16 @llvm.bswap.i16(i16
// LITTLE: call i32 @llvm.bswap.i32(i32
// LITTLE-NEXT: store i32

void test_array_access(struct ArrayStruct *s) {
  int x = s->arr[0];
  short y = s->sarr[3];
  s->arr[1] = 42;
}

// Test nested struct (inner struct should not be affected)
struct Inner {
  int x;
};

struct __attribute__((scalar_storage_order("big-endian"))) Outer {
  int scalar;
  struct Inner inner;
};

// LITTLE-LABEL: define {{.*}} @test_nested_struct
// LITTLE: load i32, ptr
// LITTLE-NEXT: call i32 @llvm.bswap.i32(i32
// LITTLE: load i32, ptr
// LITTLE-NOT: call i32 @llvm.bswap.i32

void test_nested_struct(struct Outer *s, int *a, int *b) {
  *a = s->scalar;   // Should byte-swap
  *b = s->inner.x;  // Should NOT byte-swap (inner struct not affected)
}

// Test that pointers are not byte-swapped
struct __attribute__((scalar_storage_order("big-endian"))) PtrStruct {
  int *ptr;
  int value;
};

// LITTLE-LABEL: define {{.*}} @test_pointer
// LITTLE: load ptr, ptr
// LITTLE-NOT: call {{.*}} @llvm.bswap
// LITTLE: load i32, ptr
// LITTLE-NEXT: call i32 @llvm.bswap.i32

void test_pointer(struct PtrStruct *s, int **p, int *v) {
  *p = s->ptr;     // Should NOT byte-swap (pointer)
  *v = s->value;   // Should byte-swap (int)
}
