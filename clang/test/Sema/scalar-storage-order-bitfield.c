// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fsyntax-only -verify %s

// Bit-field application (should be rejected: attribute applies to the declared type of the bit-field, here treated as part of a field declarator context).
struct BF {
  unsigned int __attribute__((scalar_storage_order("big-endian"))) bf:16; // expected-error {{'scalar_storage_order' attribute only applies to scalar integer or floating types}}
};

// Direct application to an array type (rejected).
typedef int arr_t[4];
arr_t __attribute__((scalar_storage_order("big-endian"))) bad_arr; // expected-error {{'scalar_storage_order' attribute only applies to scalar integer or floating types}}
