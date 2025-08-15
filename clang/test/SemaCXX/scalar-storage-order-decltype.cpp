// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -fsyntax-only -verify %s

typedef unsigned int __attribute__((scalar_storage_order("big-endian"))) be32_t;

be32_t gv;

// decltype(id) should preserve the attribute.
typedef decltype(gv) be32_alias;
be32_alias x = 0; // expected-no-diagnostics

// Deep typedef chain conflict.
using A1 = be32_t;
using A2 = A1 __attribute__((scalar_storage_order("big-endian"))); // duplicate ok
using A3 = A2 __attribute__((scalar_storage_order("little-endian"))); // expected-error {{'scalar_storage_order' attribute with order 'little-endian' conflicts with previous specification 'big-endian'}}
