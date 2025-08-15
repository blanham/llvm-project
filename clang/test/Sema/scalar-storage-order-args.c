// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fsyntax-only -verify %s

// Good reference (control - no diagnostics expected)
typedef unsigned int __attribute__((scalar_storage_order("little-endian"))) good_le32;

// Wrong case
typedef unsigned int __attribute__((scalar_storage_order("Big-Endian"))) bad_case; // expected-error {{'scalar_storage_order' is an invalid argument to attribute}}

// Empty string
typedef unsigned int __attribute__((scalar_storage_order(""))) bad_empty; // expected-error {{'scalar_storage_order' is an invalid argument to attribute}}

// Unquoted identifier
typedef unsigned int __attribute__((scalar_storage_order(big-endian))) bad_unquoted; // expected-error {{'scalar_storage_order' attribute takes one argument}}

// Numeric literal
typedef unsigned int __attribute__((scalar_storage_order(123))) bad_number; // expected-error {{'scalar_storage_order' attribute takes one argument}}

// Macro expanding to unquoted identifier
#define SSO_UNQUOTED big-endian
typedef unsigned int __attribute__((scalar_storage_order(SSO_UNQUOTED))) bad_macro_unquoted; // expected-error {{'scalar_storage_order' attribute takes one argument}}

// Macro expanding to quoted wrong-case string
#define SSO_WRONG "Big-Endian"
typedef unsigned int __attribute__((scalar_storage_order(SSO_WRONG))) bad_macro_case; // expected-error {{'scalar_storage_order' is an invalid argument to attribute}}

// Macro expanding to empty string
#define SSO_EMPTY ""
typedef unsigned int __attribute__((scalar_storage_order(SSO_EMPTY))) bad_macro_empty; // expected-error {{'scalar_storage_order' is an invalid argument to attribute}}

// Two arguments via macro pieces
#define SSO_LE "little-endian"
#define SSO_BE "big-endian"
typedef unsigned int __attribute__((scalar_storage_order(SSO_LE, SSO_BE))) bad_two_args; // expected-error {{'scalar_storage_order' attribute takes one argument}}

// C++ attribute spelling (still wrong case)
#ifdef __cplusplus
typedef unsigned int [[gnu::scalar_storage_order("Big-Endian")]] bad_cpp_case; // expected-error {{'scalar_storage_order' is an invalid argument to attribute}}
#endif
