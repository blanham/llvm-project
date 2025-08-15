// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fsyntax-only -verify %s

// vector type
typedef int __attribute__((vector_size(16))) vec4i;
typedef vec4i __attribute__((scalar_storage_order("big-endian"))) bad_vec; // expected-error {{'scalar_storage_order' attribute only applies to scalar integer or floating types}}

// union type
union U { int a; float b; };
typedef union U __attribute__((scalar_storage_order("little-endian"))) bad_union; // expected-error {{'scalar_storage_order' attribute only applies to scalar integer or floating types}}

// atomic type
#if __has_extension(c_atomic)
_Atomic(int) __attribute__((scalar_storage_order("big-endian"))) bad_atomic_var; // expected-error {{'scalar_storage_order' attribute only applies to scalar integer or floating types}}
#endif

// function type (redundant with earlier negative file but kept for completeness)
typedef int (__attribute__((scalar_storage_order("big-endian"))) *fnptr)(int); // expected-error {{'scalar_storage_order' attribute only applies to scalar integer or floating types}}
