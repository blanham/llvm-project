// Experimental micro-benchmark scaffold for scalar_storage_order attribute.
// Not wired into automated test suite; kept for manual performance exploration.
// Can be cherry-picked out before upstreaming if deemed out of scope.
// Build example (native):
//   clang -O3 -DSIZE=1048576 scalar_storage_order_bench.c -o sso_bench && ./sso_bench
// (Consider pinning affinity and repeating to reduce noise.)

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifndef SIZE
#define SIZE 262144
#endif

typedef unsigned int __attribute__((scalar_storage_order("big-endian"))) be32_t;

static uint32_t sink;

static inline uint64_t nsec_now(void) {
  struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

uint32_t sum_attr(const be32_t *p, size_t n) {
  uint32_t s = 0;
  for (size_t i=0;i<n;++i) s += p[i];
  return s;
}

uint32_t sum_manual(const uint32_t *p, size_t n) {
  uint32_t s = 0;
  for (size_t i=0;i<n;++i) s += __builtin_bswap32(p[i]);
  return s;
}

uint32_t sum_native(const uint32_t *p, size_t n) {
  uint32_t s = 0;
  for (size_t i=0;i<n;++i) s += p[i];
  return s;
}

int main(void) {
  uint32_t *buf = (uint32_t*)aligned_alloc(64, SIZE * sizeof(uint32_t));
  if (!buf) return 1;
  for (size_t i=0;i<SIZE;++i) buf[i] = (uint32_t)i;

  // Treat underlying memory as big-endian storage for be32_t.
  be32_t *abuf = (be32_t*)buf;

  // Warm-up
  for (int w=0; w<3; ++w) sink += sum_attr(abuf, SIZE/16);

  uint64_t t1, t2;

  t1 = nsec_now(); sink += sum_attr(abuf, SIZE); t2 = nsec_now();
  double attr_ns_per = (double)(t2 - t1)/SIZE;

  t1 = nsec_now(); sink += sum_manual(buf, SIZE); t2 = nsec_now();
  double manual_ns_per = (double)(t2 - t1)/SIZE;

  t1 = nsec_now(); sink += sum_native(buf, SIZE); t2 = nsec_now();
  double native_ns_per = (double)(t2 - t1)/SIZE;

  printf("N=%zu\nattr   = %.3f ns/elt\nmanual = %.3f ns/elt\nnative = %.3f ns/elt\n", (size_t)SIZE,
         attr_ns_per, manual_ns_per, native_ns_per);
  free(buf);
  return (int)sink; // prevent optimization
}
