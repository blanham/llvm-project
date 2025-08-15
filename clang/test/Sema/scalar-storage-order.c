// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fsyntax-only %s

typedef unsigned short __attribute__((scalar_storage_order("big-endian"))) be16_t;

be16_t ok_var;
