// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -o - %s | FileCheck %s

typedef unsigned short __attribute__((scalar_storage_order("big-endian"))) be16_t;
typedef unsigned int __attribute__((scalar_storage_order("big-endian"))) be32_t;

be16_t load16(be16_t *p) {
	// CHECK-LABEL: define{{.*}} i16 @load16
	// CHECK: load i16, ptr
	// CHECK: call i16 @llvm.bswap.i16
	return *p;
}

void store16(be16_t *p, be16_t v) {
	// CHECK-LABEL: define{{.*}} void @store16
	// CHECK: call i16 @llvm.bswap.i16
	// CHECK: store i16
	*p = v;
}

be32_t load32(be32_t *p) {
	// CHECK-LABEL: define{{.*}} i32 @load32
	// CHECK: load i32, ptr
	// CHECK: call i32 @llvm.bswap.i32
	return *p;
}

void store32(be32_t *p, be32_t v) {
	// CHECK-LABEL: define{{.*}} void @store32
	// CHECK: call i32 @llvm.bswap.i32
	// CHECK: store i32
	*p = v;
}
