// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -ast-dump -fsyntax-only %s | FileCheck %s

typedef unsigned int __attribute__((scalar_storage_order("big-endian"))) be32_t;
typedef float __attribute__((scalar_storage_order("little-endian"))) lefloat;

be32_t v1;
lefloat v2;

// CHECK: TypedefDecl {{.*}} be32_t 'unsigned int __attribute__((scalar_storage_order("big-endian")))'
// CHECK: TypedefDecl {{.*}} lefloat 'float __attribute__((scalar_storage_order("little-endian")))'
// CHECK: VarDecl {{.*}} v1 'be32_t'{{.*}}
// CHECK: VarDecl {{.*}} v2 'lefloat'{{.*}}
