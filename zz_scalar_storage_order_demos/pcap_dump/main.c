// Shock & Awe PCAP demo: richer protocol dissection (Ethernet/IP/TCP/UDP/ARP/DNS/HTTP) using scalar_storage_order.
// Not for upstream. Demonstrates attributed field decoding vs manual bswap, plus simple profiling harness.
// Build examples:
//   clang -O2 -Wall -Wextra -std=c11 pcap_dump/main.c -o pcap_dump
//   clang -O2 -Wall -Wextra -std=c11 -fno-strict-aliasing pcap_dump/main.c -o pcap_dump_nosa
// Profiling mode: add --bench=N to process only first N packets repeatedly (looping) for timing.
// Usage: ./pcap_dump <file.pcap> [--manual] [--bench=ITER] [--limit=N]
// Environment variables:
//   SSO_DEMO_SPIN=1  (optional extra CPU spin to magnify differences)

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>

#include "../common/attr_endian.h"

struct pcap_hdr_common {
    uint32_t magic_number;   // magic number
    uint16_t version_major;  // major version number
    uint16_t version_minor;  // minor version number
    int32_t  thiszone;       // GMT to local correction
    uint32_t sigfigs;        // accuracy of timestamps
    uint32_t snaplen;        // max length of captured packets, in octets
    uint32_t network;        // data link type
};

// Attributed variants (layout identical, but annotated fields for automatic byte swap).
struct pcap_hdr_be_attr {
    be32 magic_number;
    be16 version_major;
    be16 version_minor;
    int32_t thiszone;   // zone is signed; leave as native (spec defines as int32 little or big depending on magic) -- we could wrap if needed
    be32 sigfigs;
    be32 snaplen;
    be32 network;
};

struct pcap_hdr_le_attr {
    le32 magic_number;
    le16 version_major;
    le16 version_minor;
    int32_t thiszone; // native
    le32 sigfigs;
    le32 snaplen;
    le32 network;
};

struct pcaprec_hdr_common { uint32_t ts_sec, ts_usec, incl_len, orig_len; };

struct pcaprec_hdr_be_attr {
    be32 ts_sec;
    be32 ts_usec;
    be32 incl_len;
    be32 orig_len;
};

struct pcaprec_hdr_le_attr {
    le32 ts_sec;
    le32 ts_usec;
    le32 incl_len;
    le32 orig_len;
};

static int use_manual = 0;
static size_t bench_iter = 0; // number of iterations for benchmarking (reparse first limit packets)
static size_t limit_packets = (size_t)-1;

// Simple spin (optional) to amplify time differences if needed.
static void maybe_spin(void){
    if(!getenv("SSO_DEMO_SPIN")) return; volatile uint64_t x=0; for(int i=0;i<1000;i++) x+=i; }

// Ethernet / IPv4 / IPv6 / TCP / UDP / ARP / DNS (expanded) / HTTP / TLS SNI / QUIC heuristics.

struct eth_hdr_be_attr { be16 dst_hi; be32 dst_lo_src_hi; be16 src_mid; be16 ethertype; };
// We'll instead just treat as raw for readability later; proper Ethernet splits are unnecessary here.

// IPv4 header (first 20 bytes no options) big-endian fields.
struct ipv4_hdr_be_attr {
    uint8_t v_ihl; uint8_t tos; be16 total_len; be16 id; be16 frag_off;
    uint8_t ttl; uint8_t proto; be16 hdr_checksum; be32 src; be32 dst;
};

struct ipv6_hdr_be_attr {
    be32 v_tc_fl; // version(4) | traffic class(8) | flow label(20)
    be16 payload_len; uint8_t next_header; uint8_t hop_limit; unsigned char src[16]; unsigned char dst[16];
};

struct icmpv4_hdr_be_attr { uint8_t type; uint8_t code; be16 checksum; be16 rest0; be16 rest1; };
struct icmpv6_hdr_be_attr { uint8_t type; uint8_t code; be16 checksum; be32 rest; };

struct tcp_hdr_be_attr { be16 src_port; be16 dst_port; be32 seq; be32 ack; uint8_t off_res; uint8_t flags; be16 win; be16 cksum; be16 urgptr; };
struct udp_hdr_be_attr { be16 src_port; be16 dst_port; be16 len; be16 cksum; };
struct arp_hdr_be_attr { be16 htype; be16 ptype; uint8_t hlen; uint8_t plen; be16 oper; };

static void decode_dns(const unsigned char *payload, size_t len){
    if(len < 12) return; // header
    be16 id; memcpy(&id, payload, 2);
    unsigned qd = (payload[4]<<8)|payload[5]; unsigned an=(payload[6]<<8)|payload[7];
    printf(" DNS(id=%u qd=%u an=%u)", (unsigned)id, qd, an);
}

static void decode_http(const unsigned char *payload, size_t len){
    // crude heuristic: starts with uppercase letters or 'GET','POST'
    if(len<4) return; if(!memcmp(payload, "GET ",4) || !memcmp(payload,"POST",4)) {
        printf(" HTTP(%.*s)", (int)(len>4?4:len), payload);
    }
}

static void decode_tls_sni(const unsigned char *data, size_t len){
    // Very minimal TLS ClientHello SNI extraction; expect handshake type 0x01 after record header.
    if(len < 5) return; // TLS record header
    if(data[0] != 0x16) return; // Handshake
    size_t rec_len = ((size_t)data[3]<<8)|data[4]; if(rec_len+5 > len) return;
    if(len < 5+4) return; // Handshake header
    const unsigned char *hs = data+5; if(hs[0] != 0x01) return; // ClientHello
    size_t hs_len = ((size_t)hs[1]<<16)|((size_t)hs[2]<<8)|hs[3]; if(4+hs_len > rec_len) return;
    size_t p=4; if(p+2>hs_len) return; // skip version
    p+=2; // version
    if(p+32>hs_len) return; p+=32; // random
    if(p>=hs_len) return; uint8_t sid_len=hs[p]; p+=1+sid_len; if(p>hs_len) return;
    if(p+2>hs_len) return; size_t cs_len = ((size_t)hs[p]<<8)|hs[p+1]; p+=2+cs_len; if(p>hs_len) return;
    if(p>=hs_len) return; uint8_t comp_methods=hs[p]; p+=1+comp_methods; if(p>hs_len) return;
    if(p+2>hs_len) return; size_t ext_total = ((size_t)hs[p]<<8)|hs[p+1]; p+=2; if(p+ext_total>hs_len) return;
    size_t extp=p; while(extp+4 <= p+ext_total){ uint16_t etype=(hs[extp]<<8)|hs[extp+1]; uint16_t elen=(hs[extp+2]<<8)|hs[extp+3]; extp+=4; if(extp+elen>p+ext_total) break; if(etype==0){ // server_name
            if(elen < 5) break; size_t list_len = (hs[extp]<<8)|hs[extp+1]; size_t sp=extp+2; if(sp+list_len>extp+elen) break; if(sp+3>extp+elen) break; uint8_t name_type=hs[sp]; if(name_type!=0){ /* host_name only */ break; } uint16_t host_len=(hs[sp+1]<<8)|hs[sp+2]; if(sp+3+host_len>extp+elen) break; printf(" TLS(SNI=%.*s)", host_len, (const char*)(hs+sp+3)); break; }
        extp+=elen; }
}

static void decode_quic(const unsigned char *data, size_t len){
    if(len < 6) return; // minimal
    uint8_t first = data[0]; if((first & 0xC0) != 0xC0) return; // long header
    uint32_t ver = (data[1]<<24)|(data[2]<<16)|(data[3]<<8)|data[4];
    printf(" QUIC(ver=%08x)", ver);
}

static void proto_dispatch(const unsigned char *frame, size_t flen, int is_be){
    if(flen < 14) return;
    const unsigned char *eth = frame;
    uint16_t ethertype = (uint16_t)(eth[12]<<8 | eth[13]);
    const unsigned char *l3 = frame + 14; size_t l3len = flen - 14;
    if(ethertype == 0x0800 && l3len >= 20){ // IPv4
        const struct ipv4_hdr_be_attr *ip = (const struct ipv4_hdr_be_attr*)l3; // attributed
        uint8_t ihl = ip->v_ihl & 0x0F; size_t ip_hlen = ihl*4; if(ip_hlen < 20 || ip_hlen > l3len) return;
        printf(" IPv4(proto=%u src=%u.%u.%u.%u dst=%u.%u.%u.%u)", ip->proto,
               (unsigned)((ip->src>>24)&0xFF),(unsigned)((ip->src>>16)&0xFF),(unsigned)((ip->src>>8)&0xFF),(unsigned)(ip->src&0xFF),
               (unsigned)((ip->dst>>24)&0xFF),(unsigned)((ip->dst>>16)&0xFF),(unsigned)((ip->dst>>8)&0xFF),(unsigned)(ip->dst&0xFF));
        const unsigned char *l4 = l3 + ip_hlen; size_t l4len = l3len - ip_hlen;
        if(ip->proto == 6 && l4len >= 20){ // TCP
            const struct tcp_hdr_be_attr *tcp = (const struct tcp_hdr_be_attr*)l4;
            uint16_t sport = tcp->src_port, dport = tcp->dst_port;
            printf(" TCP(%u->%u)", sport, dport);
            size_t doff = (tcp->off_res>>4)*4; if(doff <= l4len) {
                const unsigned char *app = l4 + doff; size_t app_len = l4len - doff;
                if(dport==80 || sport==80) decode_http(app, app_len);
                if(dport==53 || sport==53) decode_dns(app, app_len);
                if(dport==443 || sport==443) decode_tls_sni(app, app_len);
            }
        } else if(ip->proto == 17 && l4len >= 8){ // UDP
            const struct udp_hdr_be_attr *udp = (const struct udp_hdr_be_attr*)l4;
            printf(" UDP(%u->%u)", (unsigned)udp->src_port, (unsigned)udp->dst_port);
            const unsigned char *app = l4 + 8; size_t app_len = l4len - 8;
            if(udp->dst_port==53 || udp->src_port==53) decode_dns(app, app_len);
            if(udp->dst_port==443 || udp->src_port==443) decode_quic(app, app_len);
        } else if(ip->proto == 1 && l4len >= 8){ // ICMPv4
            const struct icmpv4_hdr_be_attr *ic = (const struct icmpv4_hdr_be_attr*)l4;
            printf(" ICMPv4(type=%u code=%u)", ic->type, ic->code);
        }
    } else if(ethertype == 0x0806 && l3len >= 28){ // ARP
        const struct arp_hdr_be_attr *arp = (const struct arp_hdr_be_attr*)l3;
        printf(" ARP(op=%u)", (unsigned)arp->oper);
    } else if(ethertype == 0x86DD && l3len >= 40){ // IPv6
        const struct ipv6_hdr_be_attr *ip6 = (const struct ipv6_hdr_be_attr*)l3;
        uint32_t v_tc_fl = ip6->v_tc_fl; uint8_t version = (uint8_t)((v_tc_fl>>28)&0xF); if(version!=6) return;
        printf(" IPv6(nh=%u src=%02x%02x:%02x%02x:%02x%02x:%02x%02x ...)", ip6->next_header,
               ip6->src[0],ip6->src[1],ip6->src[2],ip6->src[3],ip6->src[4],ip6->src[5],ip6->src[6],ip6->src[7]);
        const unsigned char *l4 = l3 + 40; size_t l4len = l3len - 40; uint8_t nh=ip6->next_header;
        if(nh==6 && l4len>=20){ // TCP
            const struct tcp_hdr_be_attr *tcp = (const struct tcp_hdr_be_attr*)l4;
            printf(" TCP(%u->%u)", (unsigned)tcp->src_port,(unsigned)tcp->dst_port);
            size_t doff=(tcp->off_res>>4)*4; if(doff<=l4len){ const unsigned char *app=l4+doff; size_t app_len=l4len-doff; if(tcp->dst_port==443||tcp->src_port==443) decode_tls_sni(app,app_len); }
        } else if(nh==17 && l4len>=8){ const struct udp_hdr_be_attr *udp=(const struct udp_hdr_be_attr*)l4; printf(" UDP(%u->%u)", (unsigned)udp->src_port,(unsigned)udp->dst_port); const unsigned char *app=l4+8; size_t app_len=l4len-8; if(udp->dst_port==53||udp->src_port==53) decode_dns(app,app_len); if(udp->dst_port==443||udp->src_port==443) decode_quic(app,app_len); }
        else if(nh==58 && l4len>=4){ const struct icmpv6_hdr_be_attr *ic6=(const struct icmpv6_hdr_be_attr*)l4; printf(" ICMPv6(type=%u code=%u)", ic6->type, ic6->code); }
    }
}

static int read_fully(FILE *f, void *buf, size_t n){
    return fread(buf,1,n,f)==n ? 0 : -1;
}

static void dump_packet(size_t idx, uint32_t ts_sec, uint32_t ts_usec, uint32_t incl_len, uint32_t orig_len, const unsigned char *data) {
    printf("[#%zu] %" PRIu32 ".%06" PRIu32 " len=%" PRIu32 " orig=%" PRIu32, idx, ts_sec, ts_usec, incl_len, orig_len);
    if(data && incl_len) proto_dispatch(data, incl_len, 0);
    puts("");
}

static int process_manual(FILE *f){
    struct pcap_hdr_common gh;
    if (read_fully(f,&gh,sizeof gh)) return -1;
    uint32_t magic = gh.magic_number;
    int is_le = 0, is_be = 0;
    if (magic == 0xa1b2c3d4u) { is_be = 1; }
    else if (magic == 0xd4c3b2a1u) { is_le = 1; }
    else { fprintf(stderr,"Unknown magic %08x\n", magic); return -1; }
    uint16_t vmaj = gh.version_major;
    uint16_t vmin = gh.version_minor;
    if (is_be) { vmaj = mz_bswap16(vmaj); vmin = mz_bswap16(vmin); }
    if (is_le) { /* file little-endian matches host little-endian, no swap */ }
    uint32_t snap = gh.snaplen; if (is_be) snap = mz_bswap32(snap);
    printf("PCAP v%u.%u snaplen=%u (manual path)\n", vmaj, vmin, snap);

    unsigned char *payload = NULL; size_t idx=0;
    while (idx < limit_packets) {
        struct pcaprec_hdr_common ph;
        if (read_fully(f,&ph,sizeof ph)) break;
        uint32_t ts_sec=ph.ts_sec, ts_usec=ph.ts_usec, incl=ph.incl_len, orig=ph.orig_len;
        if (is_be) { ts_sec=mz_bswap32(ts_sec); ts_usec=mz_bswap32(ts_usec); incl=mz_bswap32(incl); orig=mz_bswap32(orig); }
        if(!payload || incl > 65536){ free(payload); payload = malloc(incl?incl:1); }
        if(incl && fread(payload,1,incl,f)!=incl) break;
        dump_packet(idx++, ts_sec, ts_usec, incl, orig, payload);
        maybe_spin();
    }
    free(payload);
    return 0;
}

static int process_attr(FILE *f){
    unsigned char hdr[sizeof(struct pcap_hdr_common)];
    if (read_fully(f,hdr,sizeof hdr)) return -1;

    // Peek magic to choose attributed interpretation then cast.
    uint32_t magic_raw; memcpy(&magic_raw, hdr, 4);
    int is_be = magic_raw == 0xa1b2c3d4u;
    int is_le = magic_raw == 0xd4c3b2a1u;
    if (!is_be && !is_le){ fprintf(stderr,"Unknown magic %08x\n", magic_raw); return -1; }

    if (is_be) {
        const struct pcap_hdr_be_attr *gh = (const struct pcap_hdr_be_attr*)hdr;
        printf("PCAP v%u.%u snaplen=%u (attr BE)\n", (unsigned)gh->version_major, (unsigned)gh->version_minor, (unsigned)gh->snaplen);
    } else {
        const struct pcap_hdr_le_attr *gh = (const struct pcap_hdr_le_attr*)hdr;
        printf("PCAP v%u.%u snaplen=%u (attr LE)\n", (unsigned)gh->version_major, (unsigned)gh->version_minor, (unsigned)gh->snaplen);
    }

    // Packet loop.
    unsigned char recbuf[sizeof(struct pcaprec_hdr_common)];
    unsigned char *payload = NULL;
    size_t idx=0;
    while (idx < limit_packets && !read_fully(f, recbuf, sizeof recbuf)) {
        uint32_t incl_len=0, orig_len=0, ts_sec=0, ts_usec=0;
        if (is_be) {
            const struct pcaprec_hdr_be_attr *ph = (const struct pcaprec_hdr_be_attr*)recbuf;
            ts_sec=ph->ts_sec; ts_usec=ph->ts_usec; incl_len=ph->incl_len; orig_len=ph->orig_len;
        } else {
            const struct pcaprec_hdr_le_attr *ph = (const struct pcaprec_hdr_le_attr*)recbuf;
            ts_sec=ph->ts_sec; ts_usec=ph->ts_usec; incl_len=ph->incl_len; orig_len=ph->orig_len;
        }
        if(!payload || incl_len > 65536){ free(payload); payload = malloc(incl_len?incl_len:1); }
        if(incl_len && fread(payload,1,incl_len,f)!=incl_len) break;
        dump_packet(idx++, ts_sec, ts_usec, incl_len, orig_len, payload);
        maybe_spin();
    }
    free(payload);
    return 0;
}

static double now_sec(void){ struct timeval tv; gettimeofday(&tv,NULL); return tv.tv_sec + tv.tv_usec/1e6; }

int main(int argc, char **argv){
    if (argc < 2) { fprintf(stderr,"Usage: %s <file.pcap> [--manual] [--bench=ITER] [--limit=N]\n", argv[0]); return 1; }
    const char *path = argv[1];
    for(int i=2;i<argc;i++){
        if(!strcmp(argv[i],"--manual")) use_manual=1;
        else if(!strncmp(argv[i],"--bench=",8)) bench_iter = strtoull(argv[i]+8,NULL,10);
        else if(!strncmp(argv[i],"--limit=",8)) limit_packets = strtoull(argv[i]+8,NULL,10);
    }
    if(!bench_iter){
        FILE *f = fopen(path,"rb"); if(!f){ perror("fopen"); return 1; }
        int rc = use_manual ? process_manual(f) : process_attr(f);
        fclose(f); return rc;
    }
    // Benchmark loop: re-run parsing first limit_packets packets bench_iter times.
    double start = now_sec();
    for(size_t it=0; it<bench_iter; ++it){
        FILE *f = fopen(path,"rb"); if(!f){ perror("fopen"); return 1; }
        (use_manual ? process_manual : process_attr)(f);
        fclose(f);
    }
    double end = now_sec();
    printf("BENCH mode iterations=%zu total=%.3fs per=%.6fs mode=%s\n", bench_iter, end-start, (end-start)/bench_iter, use_manual?"manual":"attr");
    return 0;
}
