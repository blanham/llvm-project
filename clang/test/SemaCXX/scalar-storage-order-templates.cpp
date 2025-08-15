// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -fsyntax-only -verify %s

// Basic template using GNU-style attribute spelling.
template <typename T>
using BEWrap = T [[gnu::scalar_storage_order("big-endian")]];

BEWrap<unsigned int> be_u32; // ok

// Ensure duplicate identical attributes via typedef layering are accepted silently.
using BE1 = unsigned int [[gnu::scalar_storage_order("big-endian")]];
using BE2 = BE1 [[gnu::scalar_storage_order("big-endian")]]; // ok duplicate
BE2 be_dup;

// Conflict through typedef layering inside a template.
template <typename T>
using LEWrap = T [[gnu::scalar_storage_order("little-endian")]];

// Expected error: applying little-endian then big-endian.
using Conflict = LEWrap<unsigned short> [[gnu::scalar_storage_order("big-endian")]]; // expected-error {{'scalar_storage_order' attribute with order 'big-endian' conflicts with previous specification 'little-endian'}}

// Attribute on template parameter type alias instantiation.
template <typename T>
struct Holder {
  using inner_be = T [[gnu::scalar_storage_order("big-endian")]];
  inner_be value; // ensure it's instantiated
};

Holder<unsigned short> h1; // instantiate

// Template with both spellings (GNU and C style) to ensure they are equivalent in C++ mode.
template <typename T>
using LEWrapCStyle = T __attribute__((scalar_storage_order("little-endian")));

LEWrapCStyle<unsigned int> le32_ok; // ok

// Mixing spellings while duplicating same order should be silent.
using LEBase = unsigned int __attribute__((scalar_storage_order("little-endian")));
using LEDerived = LEBase [[gnu::scalar_storage_order("little-endian")]]; // ok duplicate, mixed spelling
LEDerived le_dup;

// Conflicting mixed spelling.
using ConflictMix = LEBase [[gnu::scalar_storage_order("big-endian")]]; // expected-error {{'scalar_storage_order' attribute with order 'big-endian' conflicts with previous specification 'little-endian'}}

// Ensure attribute does not apply to non-scalar template parameter instantiations.
struct S { int x; };
using Bad = S [[gnu::scalar_storage_order("big-endian")]]; // expected-error {{'scalar_storage_order' attribute only applies to scalar integer or floating types}}
