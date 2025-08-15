// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -fsyntax-only -verify %s

typedef unsigned short [[gnu::scalar_storage_order("big-endian")]] be16_t;

be16_t ok_var2;
