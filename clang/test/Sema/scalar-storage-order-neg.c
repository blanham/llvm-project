// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fsyntax-only -verify %s

// Basic valid typedef
typedef unsigned int __attribute__((scalar_storage_order("little-endian"))) le32_t;

// Invalid argument string
typedef unsigned int __attribute__((scalar_storage_order("middle"))) bad_arg; // expected-error {{'scalar_storage_order' is an invalid argument to attribute}}

// Wrong number of arguments
typedef unsigned int __attribute__((scalar_storage_order())) no_arg; // expected-error {{'scalar_storage_order' attribute takes one argument}}
typedef unsigned int __attribute__((scalar_storage_order("little-endian","big-endian"))) two_args; // expected-error {{'scalar_storage_order' attribute takes one argument}}

// Not on non-scalar (struct)
struct S { int x; };
typedef struct S __attribute__((scalar_storage_order("big-endian"))) bad_struct; // expected-error {{'scalar_storage_order' attribute only applies to scalar integer or floating types}}

// Not on pointer
typedef int * __attribute__((scalar_storage_order("big-endian"))) bad_ptr; // expected-error {{'scalar_storage_order' attribute only applies to scalar integer or floating types}}

// Not on function type (attribute placed within function pointer declarator)
typedef int (__attribute__((scalar_storage_order("big-endian"))) *fnptr)(int); // expected-error {{'scalar_storage_order' attribute only applies to scalar integer or floating types}}
