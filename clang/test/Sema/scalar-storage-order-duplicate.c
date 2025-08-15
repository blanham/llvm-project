// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fsyntax-only -verify %s

// Duplicate with same argument: second silently ignored.
typedef unsigned int __attribute__((scalar_storage_order("big-endian"), scalar_storage_order("big-endian"))) be32_same;
be32_same test_same(be32_same v) { return v; }

// Conflicting duplicates: error on second.
typedef unsigned int __attribute__((scalar_storage_order("big-endian"), scalar_storage_order("little-endian"))) be32_conflict; // expected-error {{'scalar_storage_order' attribute with order 'little-endian' conflicts with previous specification 'big-endian'}}

// Mixed through typedef layering.
typedef unsigned short __attribute__((scalar_storage_order("little-endian"))) base_le16;
typedef base_le16 __attribute__((scalar_storage_order("little-endian"))) still_le16; // ok duplicate

typedef base_le16 __attribute__((scalar_storage_order("big-endian"))) mismatch16; // expected-error {{'scalar_storage_order' attribute with order 'big-endian' conflicts with previous specification 'little-endian'}}
