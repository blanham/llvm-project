// RUN: %clang_cc1 -E -dM -x c %s | FileCheck %s

#if __has_attribute(scalar_storage_order)
#define HAS_SSO 1
#endif

// CHECK: #define HAS_SSO 1
