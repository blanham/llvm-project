// Full PNG loader (core features) with attributed endian fields for chunk lengths & IHDR.
// Supports: IHDR, IDAT (zlib inflate via miniz-like tiny inflater here), no interlace, 8-bit RGBA/RGB/Gray, PLTE optional, ignoring ancillary.
// This is a simplified educational decoder; not hardened. Remove before upstream.
#include "image_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <zlib.h> // Rely on system zlib; if unavailable, this file would need a tiny inflater.

struct png_chunk_attr { be32 length; char type[4]; };
struct png_ihdr_attr { be32 width; be32 height; uint8_t bit_depth; uint8_t color_type; uint8_t compression; uint8_t filter; uint8_t interlace; };
struct png_chunk_manual { uint32_t length; char type[4]; };
struct png_ihdr_manual { uint32_t width,height; uint8_t bit_depth,color_type,compression,filter,interlace; };

static int read_be32(FILE *f, uint32_t *out){ unsigned char b[4]; if(fread(b,1,4,f)!=4) return -1; *out=(uint32_t)(b[0]<<24|b[1]<<16|b[2]<<8|b[3]); return 0; }

static int load_png_stream(FILE *f, int manual, int decode_pixels, ImageData *out){
    unsigned char sig[8]; if(fread(sig,1,8,f)!=8) return -1; unsigned char ref[8]={137,80,78,71,13,10,26,10}; if(memcmp(sig,ref,8)) return -1;
    uint32_t w=0,h=0; int color_type=0, bit_depth=0; int gotIHDR=0;
    unsigned char *compressed=NULL; size_t compSize=0, compCap=0;
    while(1){
        long chunk_pos = ftell(f);
        unsigned char hdr[8]; if(fread(hdr,1,8,f)!=8) { goto fail; }
        uint32_t length; char type[5]={0};
        if(manual){ length = (uint32_t)(hdr[0]<<24|hdr[1]<<16|hdr[2]<<8|hdr[3]); memcpy(type,&hdr[4],4); }
        else { const struct png_chunk_attr *ch=(const struct png_chunk_attr*)hdr; length = ch->length; memcpy(type,ch->type,4); }
        if(length > (1u<<24)) { fprintf(stderr,"PNG chunk too big\n"); goto fail; }
        unsigned char *data = (unsigned char*)malloc(length); if(length && fread(data,1,length,f)!=length){ free(data); goto fail; }
        unsigned char crc[4]; if(fread(crc,1,4,f)!=4){ free(data); goto fail; }
        if(!strcmp(type,"IHDR")){
            if(length!=13){ free(data); goto fail; }
            if(manual){ struct png_ihdr_manual ih; memcpy(&ih,data,13); w=mz_bswap32(ih.width); h=mz_bswap32(ih.height); bit_depth=ih.bit_depth; color_type=ih.color_type; }
            else { const struct png_ihdr_attr *ih=(const struct png_ihdr_attr*)data; w=ih->width; h=ih->height; bit_depth=ih->bit_depth; color_type=ih->color_type; }
            gotIHDR=1; if(bit_depth!=8){ fprintf(stderr,"PNG only 8-bit supported\n"); free(data); goto fail; }
        } else if(!strcmp(type,"IDAT")){
            if(compSize+length > compCap){ size_t nc = (compCap?compCap*2:65536); while(nc < compSize+length) nc*=2; unsigned char *tmp=(unsigned char*)realloc(compressed,nc); if(!tmp){ free(data); goto fail; } compressed=tmp; compCap=nc; }
            memcpy(compressed+compSize,data,length); compSize+=length;
        } else if(!strcmp(type,"IEND")){
            free(data); break;
        }
        free(data);
    }
    if(!gotIHDR){ goto fail; }
    out->format=IMG_PNG; out->width=w; out->height=h;
    if(!decode_pixels){ free(compressed); return 0; }
    // Decompress
    z_stream zs; memset(&zs,0,sizeof zs); if(inflateInit(&zs)!=Z_OK){ goto fail; }
    zs.next_in=compressed; zs.avail_in=(uInt)compSize;
    size_t rowBytes = 1 + w * ((color_type==2?3:(color_type==6?4:1))); // filter byte + pixels
    size_t rawSize = rowBytes * h;
    unsigned char *raw = (unsigned char*)malloc(rawSize); if(!raw){ inflateEnd(&zs); goto fail; }
    zs.next_out=raw; zs.avail_out=(uInt)rawSize; int infret=inflate(&zs,Z_FINISH); if(infret!=Z_STREAM_END){ free(raw); inflateEnd(&zs); goto fail; }
    inflateEnd(&zs);
    free(compressed); compressed=NULL;
    // Unfilter (only handle filter types 0/1/2/3/4) minimal implementation
    unsigned bpp = (color_type==2?3:(color_type==6?4:1));
    unsigned char *pixels=(unsigned char*)malloc((size_t)w*h*4); if(!pixels){ free(raw); goto fail; }
    for(uint32_t y=0;y<h;y++){
        unsigned char *row = raw + y*rowBytes; unsigned ftype = row[0]; unsigned char *px = row+1;
        // Apply filters referencing prior row (prev) and prior pixel (left)
        for(uint32_t x=0;x<w*bpp;x++){
            unsigned char left = (x>=bpp)? px[x-bpp]:0;
            unsigned char up = (y? raw[(y-1)*rowBytes + 1 + x] : 0);
            unsigned char up_left = (y && x>=bpp)? raw[(y-1)*rowBytes + 1 + x - bpp]:0;
            switch(ftype){
                case 0: break;
                case 1: px[x] = (unsigned char)(px[x] + left); break;
                case 2: px[x] = (unsigned char)(px[x] + up); break;
                case 3: px[x] = (unsigned char)(px[x] + (uint8_t)((left+up)/2)); break;
                case 4: { int p = left + up - up_left; int pa=abs(p-left), pb=abs(p-up), pc=abs(p-up_left); unsigned char pr = (pa<=pb && pa<=pc)? left : (pb<=pc? up : up_left); px[x]=(unsigned char)(px[x]+pr); } break;
                default: break; // ignore
            }
        }
        // Expand to RGBA
        for(uint32_t x=0;x<w;x++){
            size_t srci = x*bpp; size_t di = ((size_t)y*w + x)*4;
            if(color_type==2){ pixels[di+0]=px[srci+0]; pixels[di+1]=px[srci+1]; pixels[di+2]=px[srci+2]; pixels[di+3]=255; }
            else if(color_type==6){ pixels[di+0]=px[srci+0]; pixels[di+1]=px[srci+1]; pixels[di+2]=px[srci+2]; pixels[di+3]=px[srci+3]; }
            else if(color_type==0){ unsigned v=px[srci]; pixels[di+0]=pixels[di+1]=pixels[di+2]=v; pixels[di+3]=255; }
            else { pixels[di+0]=pixels[di+1]=pixels[di+2]=0; pixels[di+3]=255; }
        }
    }
    free(raw); out->pixels=pixels; out->channels=4; return 0;
fail:
    free(compressed); return -1;
}

int load_png(const char *path, int decode_pixels, int manual, ImageData *out){
    memset(out,0,sizeof *out); FILE *f=fopen(path,"rb"); if(!f){ perror("fopen"); return -1; }
    int rc = load_png_stream(f,manual,decode_pixels,out); if(!rc){ out->format=IMG_PNG; }
    fclose(f); return rc;
}
