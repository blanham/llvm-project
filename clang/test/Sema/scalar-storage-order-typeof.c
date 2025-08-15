// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fsyntax-only -verify %s

typedef unsigned int __attribute__((scalar_storage_order("big-endian"))) be32_t;

be32_t g;

// GNU typeof should preserve the attributed type.
typedef typeof(g) be32_alias;
be32_alias a = 0; // expected-no-diagnostics

// typeof on an expression with usual arithmetic conversions; unary plus promotes.
// The attribute should not erroneously attach to the promoted plain int.
int c = +g; // expected-no-diagnostics
