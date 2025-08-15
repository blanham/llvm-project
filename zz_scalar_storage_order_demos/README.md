# Temporary demo programs for scalar_storage_order attribute

This directory is NOT for upstream; it provides exploratory demo / stress tests:

1. `pcap_dump`: Reads a pcap (legacy pcap, not pcap-ng) file and dumps packet summaries using attributed types for header fields.
2. `image_viewer`: Minimal PPM/PNG-like (PPM implemented, PNG stub) loader using attributed types for multi-byte fields; includes casting and memcpy stress paths.

Both programs offer modes to:
- Use attributed types with strict aliasing enabled (default).
- Force fallback path using manual `__builtin_bswap*` for comparison.
- Toggle `-fno-strict-aliasing` to observe any behavioral differences.

They are intentionally simple and rely only on standard libc.

Removal: Delete `zz_scalar_storage_order_demos/` before upstreaming final patches.
