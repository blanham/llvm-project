// Simplified JPEG decoder (baseline DCT, SOF0, no progressive, no fancy upsampling) with attributed endian markers for lengths.
// This is a non-optimized illustrative decoder focusing on endian field parsing.
// For brevity and safety, actual IDCT & Huffman decode here is skeletal; we parse dimensions.
#include "image_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_u8(FILE *f){ int c=fgetc(f); return c; }
static int read_u16_be(FILE *f){ int b1=read_u8(f); int b2=read_u8(f); if(b1<0||b2<0) return -1; return (b1<<8)|b2; }

int load_jpeg(const char *path, int decode_pixels, int manual, ImageData *out){
    (void)manual; (void)decode_pixels; memset(out,0,sizeof *out); out->format=IMG_JPEG;
    FILE *f=fopen(path,"rb"); if(!f){ perror("fopen"); return -1; }
    if(read_u8(f)!=0xFF || read_u8(f)!=0xD8){ fclose(f); return -1; }
    int width=0,height=0; int done=0;
    while(!done){
        int m1=read_u8(f); if(m1<0){ break; }
        if(m1!=0xFF) continue; // fill bytes
        int marker; do { marker=read_u8(f); } while(marker==0xFF);
        if(marker<0) break; if(marker==0xD9) break; // EOI
        if(marker==0xDA){ // SOS start of scan – stop after dimensions known
            done=1; break; }
        int seglen = read_u16_be(f); if(seglen<2){ break; }
        if(marker==0xC0){ // SOF0 baseline
            int precision=read_u8(f); height=read_u16_be(f); width=read_u16_be(f); int comps=read_u8(f); (void)precision; (void)comps; done=1;
        } else {
            if(fseek(f, seglen-2, SEEK_CUR)) break; // skip
        }
    }
    fclose(f); if(width<=0||height<=0) return -1; out->width=width; out->height=height; out->channels=3; return 0;
}
