// QOI loader (header + full decode) using attributed endian width/height.
// Specification: https://qoiformat.org/qoi-specification.pdf (simplified implementation).
#include "image_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct qoi_header_attr { char magic[4]; be32 width; be32 height; uint8_t channels; uint8_t colorspace; };
struct qoi_header_manual { char magic[4]; uint32_t width; uint32_t height; uint8_t channels; uint8_t colorspace; };

#define QOI_OP_INDEX 0x00
#define QOI_OP_DIFF  0x40
#define QOI_OP_LUMA  0x80
#define QOI_OP_RUN   0xC0
#define QOI_OP_RGB   0xFE
#define QOI_OP_RGBA  0xFF

static int read_all(FILE *f, void *b, size_t n){ return fread(b,1,n,f)==n?0:-1; }

int load_qoi(const char *path, int decode_pixels, int manual, ImageData *out){
    memset(out,0,sizeof *out); out->format=IMG_QOI;
    FILE *f=fopen(path,"rb"); if(!f){ perror("fopen"); return -1; }
    unsigned char hdrbuf[14]; if(read_all(f,hdrbuf,14)){ fclose(f); return -1; }
    if(memcmp(hdrbuf,"qoif",4)){ fclose(f); return -1; }
    uint32_t w=0,h=0; uint8_t ch=0; if(manual){ struct qoi_header_manual hman; memcpy(&hman,hdrbuf,14); w=mz_bswap32(hman.width); h=mz_bswap32(hman.height); ch=hman.channels; } else { const struct qoi_header_attr *ha=(const struct qoi_header_attr*)hdrbuf; w=ha->width; h=ha->height; ch=ha->channels; }
    if(!decode_pixels){ out->width=w; out->height=h; out->channels=ch; fclose(f); return 0; }
    size_t px_len = (size_t)w*h*4; uint8_t *pixels = (uint8_t*)malloc(px_len); if(!pixels){ fclose(f); return -1; }
    uint8_t index[64][4]={{0}}; uint8_t r=0,g=0,b=0,a=255; size_t p=0; int run=0;
    while(p < px_len){
        if(run>0){ run--; goto write_px; }
        int b0 = fgetc(f); if(b0==EOF){ free(pixels); fclose(f); return -1; }
        if(b0==QOI_OP_RGB){ r=fgetc(f); g=fgetc(f); b=fgetc(f); }
        else if(b0==QOI_OP_RGBA){ r=fgetc(f); g=fgetc(f); b=fgetc(f); a=fgetc(f); }
        else if((b0 & 0xC0)==QOI_OP_INDEX){ int idx=b0 & 0x3F; r=index[idx][0]; g=index[idx][1]; b=index[idx][2]; a=index[idx][3]; }
        else if((b0 & 0xC0)==QOI_OP_DIFF){ r=(uint8_t)(r + ((b0>>4)&0x03)-2); g=(uint8_t)(g + ((b0>>2)&0x03)-2); b=(uint8_t)(b + (b0&0x03)-2); }
        else if((b0 & 0xC0)==QOI_OP_LUMA){ int b1=fgetc(f); int dg=(b0 & 0x3F)-32; int dr_dg=((b1>>4)&0x0F)-8; int db_dg=(b1&0x0F)-8; r=(uint8_t)(r + dg + dr_dg); g=(uint8_t)(g + dg); b=(uint8_t)(b + dg + db_dg); }
        else if((b0 & 0xC0)==QOI_OP_RUN){ run = (b0 & 0x3F); }
        write_px:;
        int idx_hash = (r*3 + g*5 + b*7 + a*11) % 64; index[idx_hash][0]=r; index[idx_hash][1]=g; index[idx_hash][2]=b; index[idx_hash][3]=a;
        pixels[p++] = r; pixels[p++] = g; pixels[p++] = b; pixels[p++] = a;
    }
    out->width=w; out->height=h; out->channels=4; out->pixels=pixels; fclose(f); return 0;
}
