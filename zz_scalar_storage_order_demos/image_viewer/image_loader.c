// Multi-format image loader demo (BMP, QOI, PNG(stub), JPEG(stub)) using scalar_storage_order.
// Focus: exercising endian attributed types for headers and benchmarking attr vs manual conversion.
// Not production quality. Remove before upstream.
// Build: clang -O2 -Wall -Wextra -std=c11 image_viewer/image_loader.c -o image_loader -lm
// Usage: ./image_loader <file> [--manual] [--bench=ITER]

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>

#include "../common/attr_endian.h"

static int use_manual=0; static size_t bench_iter=0;

static double now_sec(void){ struct timeval tv; gettimeofday(&tv,NULL); return tv.tv_sec + tv.tv_usec/1e6; }

// ---------- BMP ----------
#pragma pack(push,1)
struct bmp_file_header_manual { uint16_t bfType; uint32_t bfSize; uint16_t bfReserved1; uint16_t bfReserved2; uint32_t bfOffBits; };
struct bmp_info_header_manual { uint32_t biSize; int32_t biWidth; int32_t biHeight; uint16_t biPlanes; uint16_t biBitCount; uint32_t biCompression; uint32_t biSizeImage; int32_t biXPelsPerMeter; int32_t biYPelsPerMeter; uint32_t biClrUsed; uint32_t biClrImportant; };

struct bmp_file_header_attr { be16 bfType; be32 bfSize; be16 bfReserved1; be16 bfReserved2; be32 bfOffBits; };
struct bmp_info_header_attr { be32 biSize; int32_t biWidth; int32_t biHeight; be16 biPlanes; be16 biBitCount; be32 biCompression; be32 biSizeImage; int32_t biXPelsPerMeter; int32_t biYPelsPerMeter; be32 biClrUsed; be32 biClrImportant; };
#pragma pack(pop)

static int load_bmp(FILE *f){
    unsigned char header[14+40]; if(fread(header,1,sizeof header,f)!=sizeof header) return -1;
    if(use_manual){
        struct bmp_file_header_manual fh; memcpy(&fh, header,14);
        if(fh.bfType != 0x4D42){ fprintf(stderr,"Not BMP\n"); return -1; }
        struct bmp_info_header_manual ih; memcpy(&ih, header+14,40);
        printf("BMP %dx%d bpp=%u (manual)\n", ih.biWidth, ih.biHeight, ih.biBitCount);
    } else {
        const struct bmp_file_header_attr *fh = (const struct bmp_file_header_attr*)header;
        if(fh->bfType != 0x4D42){ fprintf(stderr,"Not BMP\n"); return -1; }
        const struct bmp_info_header_attr *ih = (const struct bmp_info_header_attr*)(header+14);
        printf("BMP %dx%d bpp=%u (attr)\n", ih->biWidth, ih->biHeight, ih->biBitCount);
    }
    return 0;
}

// ---------- QOI (Quite OK Image) minimal header parse ----------
struct qoi_header_manual { char magic[4]; uint32_t width; uint32_t height; uint8_t channels; uint8_t colorspace; };
struct qoi_header_attr { char magic[4]; be32 width; be32 height; uint8_t channels; uint8_t colorspace; };

static int load_qoi(FILE *f){ unsigned char hdr[14]; if(fread(hdr,1,14,f)!=14) return -1; if(memcmp(hdr,"qoif",4)){ fprintf(stderr,"Not QOI\n"); return -1; }
    if(use_manual){ struct qoi_header_manual h; memcpy(&h,hdr,14); printf("QOI %ux%u ch=%u (manual)\n", mz_bswap32(h.width), mz_bswap32(h.height), h.channels); }
    else { const struct qoi_header_attr *h = (const struct qoi_header_attr*)hdr; printf("QOI %ux%u ch=%u (attr)\n", h->width, h->height, h->channels); }
    return 0; }

// ---------- PNG (stub: just parse signature + IHDR) ----------
struct png_chunk_manual { uint32_t length; char type[4]; };
struct png_chunk_attr { be32 length; char type[4]; };
struct png_ihdr_manual { uint32_t width, height; uint8_t bit_depth, color_type, compression, filter, interlace; };
struct png_ihdr_attr { be32 width, height; uint8_t bit_depth, color_type, compression, filter, interlace; };

static int load_png(FILE *f){ unsigned char sig[8]; if(fread(sig,1,8,f)!=8) return -1; static const unsigned char ref[8]={137,80,78,71,13,10,26,10}; if(memcmp(sig,ref,8)){ fprintf(stderr,"Not PNG\n"); return -1; }
    unsigned char chunk_hdr[8]; if(fread(chunk_hdr,1,8,f)!=8) return -1; // IHDR length+type
    if(use_manual){ struct png_chunk_manual ch; memcpy(&ch,chunk_hdr,8); uint32_t len=mz_bswap32(ch.length); if(memcmp(ch.type,"IHDR",4)){ fprintf(stderr,"PNG no IHDR first\n"); return -1; } unsigned char ihdr[13]; if(fread(ihdr,1,13,f)!=13) return -1; struct png_ihdr_manual ih; memcpy(&ih,ihdr,13); printf("PNG %ux%u (manual)\n", mz_bswap32(ih.width), mz_bswap32(ih.height)); }
    else { const struct png_chunk_attr *ch = (const struct png_chunk_attr*)chunk_hdr; if(ch->length != 13 || memcmp(ch->type,"IHDR",4)){ fprintf(stderr,"PNG no IHDR first\n"); return -1; } struct png_ihdr_attr ih; if(fread(&ih,1,13,f)!=13) return -1; printf("PNG %ux%u (attr)\n", ih.width, ih.height); }
    return 0; }

// ---------- JPEG (stub: scan SOI + dimensions via SOFn is complex; we print stub) ----------
static int load_jpeg(FILE *f){ unsigned char soi[2]; if(fread(soi,1,2,f)!=2) return -1; if(soi[0]!=0xFF||soi[1]!=0xD8){ fprintf(stderr,"Not JPEG\n"); return -1; } printf("JPEG (stub parse)\n"); return 0; }

static int dispatch(const char *path){ FILE *f=fopen(path,"rb"); if(!f){ perror("fopen"); return -1; }
    unsigned char head[16]; size_t n=fread(head,1,sizeof head,f); rewind(f);
    int rc=-1; if(n>=2 && head[0]=='B'&&head[1]=='M') rc=load_bmp(f);
    else if(n>=4 && !memcmp(head,"qoif",4)) rc=load_qoi(f);
    else if(n>=8 && head[0]==137&&head[1]==80&&head[2]==78&&head[3]==71) rc=load_png(f);
    else if(n>=2 && head[0]==0xFF && head[1]==0xD8) rc=load_jpeg(f);
    else fprintf(stderr,"Unknown format\n");
    fclose(f); return rc; }

int main(int argc,char**argv){ if(argc<2){ fprintf(stderr,"Usage: %s <image> [--manual] [--bench=ITER]\n", argv[0]); return 1; }
    for(int i=2;i<argc;i++){ if(!strcmp(argv[i],"--manual")) use_manual=1; else if(!strncmp(argv[i],"--bench=",8)) bench_iter=strtoull(argv[i]+8,NULL,10); }
    if(!bench_iter) return dispatch(argv[1]);
    double s=now_sec(); for(size_t i=0;i<bench_iter;i++) dispatch(argv[1]); double e=now_sec();
    printf("BENCH image iterations=%zu total=%.3fs per=%.6fs mode=%s\n", bench_iter, e-s, (e-s)/bench_iter, use_manual?"manual":"attr");
    return 0; }
