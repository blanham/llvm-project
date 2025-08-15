Temporary demo: pcap_dump (extended dissection + benchmarking)

Demonstrates parsing libpcap files using scalar_storage_order attributed types for header fields and protocol decoding.

Features:
  - BE & LE global headers (0xa1b2c3d4 / 0xd4c3b2a1)
  - Attribute vs manual modes (--manual)
  - Ethernet + IPv4 + (TCP/UDP) + ARP parsing
  - DNS and HTTP lightweight heuristics
  - Benchmark mode (--bench=ITER, optional --limit=N)
  - Optional spin via SSO_DEMO_SPIN=1 env var to amplify cost
  - perf-friendly output (BENCH line)

Build examples:
  clang -O2 -Wall -Wextra -std=c11 pcap_dump/main.c -o pcap_dump
  clang -O2 -Wall -Wextra -std=c11 -fno-strict-aliasing pcap_dump/main.c -o pcap_dump_nosa

Quick usage:
  tcpdump -i lo -c 5 -w test.pcap (requires root or appropriate perms)
  ./pcap_dump test.pcap | head
  ./pcap_dump test.pcap --manual | head
  ./pcap_dump test.pcap --bench=50 --limit=200
  perf stat ./pcap_dump test.pcap --bench=200 --limit=500

Removal: This directory is staging only and should not ship upstream.
