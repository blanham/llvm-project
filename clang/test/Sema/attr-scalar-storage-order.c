// RUN: %clang_cc1 -fsyntax-only -verify %s

// Test scalar_storage_order attribute parsing

struct __attribute__((scalar_storage_order("big-endian"))) S1 {
  int x;
  short y;
};

struct __attribute__((scalar_storage_order("little-endian"))) S2 {
  int x;
  short y;
};

union __attribute__((scalar_storage_order("big-endian"))) U1 {
  int x;
  short y;
};

// Test address-of restrictions for reverse storage order fields
// On little-endian systems
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

struct __attribute__((scalar_storage_order("big-endian"))) S3 {
  int scalar_field;
  int array_field[4];
  struct {
    int nested;
  } nested_struct;
};

void test_address_taking() {
  struct S3 s;

  // Error: taking address of scalar field with reverse storage order
  int *p1 = &s.scalar_field; // expected-error {{address of scalar field with reverse storage order is not allowed}}

  // Warning: taking address of array field with scalar components
  int *p2 = s.array_field; // expected-warning {{address of array field with scalar component and reverse storage order requested}}

  // OK: taking address of nested struct field (struct/union fields are not affected)
  int *p3 = &s.nested_struct.nested; // OK
}

#endif

// Test on big-endian systems
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__

struct __attribute__((scalar_storage_order("little-endian"))) S4 {
  int scalar_field;
  int array_field[4];
};

void test_address_taking_be() {
  struct S4 s;

  // Error: taking address of scalar field with reverse storage order
  int *p1 = &s.scalar_field; // expected-error {{address of scalar field with reverse storage order is not allowed}}

  // Warning: taking address of array field with scalar components
  int *p2 = s.array_field; // expected-warning {{address of array field with scalar component and reverse storage order requested}}
}

#endif

// Test that the attribute is ignored on non-record types
int __attribute__((scalar_storage_order("big-endian"))) x; // expected-warning {{'scalar_storage_order' attribute ignored}}
