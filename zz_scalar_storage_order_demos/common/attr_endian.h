// Shared endian attributed typedefs for demo programs.
// Temporary file (remove before upstreaming attribute proper).
#ifndef ZZ_SSO_DEMOS_ATTR_ENDIAN_H
#define ZZ_SSO_DEMOS_ATTR_ENDIAN_H

#include <stdint.h>

// Big-endian storage order scalars
typedef uint16_t __attribute__((scalar_storage_order("big-endian"))) be16;
typedef uint32_t __attribute__((scalar_storage_order("big-endian"))) be32;
typedef uint64_t __attribute__((scalar_storage_order("big-endian"))) be64;

// Little-endian storage order scalars
typedef uint16_t __attribute__((scalar_storage_order("little-endian"))) le16;
typedef uint32_t __attribute__((scalar_storage_order("little-endian"))) le32;
typedef uint64_t __attribute__((scalar_storage_order("little-endian"))) le64;

// Manual helpers
static inline uint16_t mz_bswap16(uint16_t v){ return (uint16_t)((v<<8)|(v>>8)); }
static inline uint32_t mz_bswap32(uint32_t v){ return __builtin_bswap32(v); }
static inline uint64_t mz_bswap64(uint64_t v){ return __builtin_bswap64(v); }

#endif // ZZ_SSO_DEMOS_ATTR_ENDIAN_H
