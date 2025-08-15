// Full PNG loader (core features) with attributed endian fields for chunk lengths & IHDR.
// Supports: IHDR, IDAT (zlib inflate via system zlib), PLTE (palette), tRNS (palette / grayscale transparency), Adam7 interlace, 8-bit modes: RGBA (6), RGB (2), Gray (0), Indexed (3), Gray+Alpha (4).
// CRC is verified (warnings counted). Ancillary chunks: gAMA, sRGB, pHYs parsed; iCCP header name parsed (profile data skipped). Others logged & skipped.
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

static uint32_t crc_table[256];
static int crc_init=0;
static void make_crc_table(void){
    for(unsigned n=0;n<256;n++){ uint32_t c=n; for(int k=0;k<8;k++){ c = (c & 1)? (0xEDB88320u ^ (c>>1)) : (c>>1); } crc_table[n]=c; }
    crc_init=1;
}
static uint32_t png_crc(const unsigned char *buf,size_t len){ if(!crc_init) make_crc_table(); uint32_t c=0xFFFFFFFFu; for(size_t i=0;i<len;i++) c = crc_table[(c ^ buf[i]) & 0xFF] ^ (c>>8); return c ^ 0xFFFFFFFFu; }

static int load_png_stream(FILE *f, int manual, int decode_pixels, ImageData *out){
    unsigned char sig[8]; if(fread(sig,1,8,f)!=8) return -1; unsigned char ref[8]={137,80,78,71,13,10,26,10}; if(memcmp(sig,ref,8)) return -1;
    uint32_t w=0,h=0; int color_type=0, bit_depth=0; int gotIHDR=0; int interlace=0;
    unsigned char *compressed=NULL; size_t compSize=0, compCap=0;
    unsigned char *palette=NULL; size_t palette_entries=0; unsigned char *trns=NULL; size_t trns_len=0; // tRNS raw data
    unsigned crc_mismatch_count=0;
    // Ancillary metadata
    double gamma_val = 0.0; int have_gamma=0; int srgb_intent=-1; uint32_t phys_ppux=0, phys_ppuy=0; int phys_unit=0; int have_phys=0;
    while(1){
        long chunk_pos = ftell(f);
        unsigned char hdr[8]; if(fread(hdr,1,8,f)!=8) { goto fail; }
        uint32_t length; char type[5]={0};
        if(manual){ length = (uint32_t)(hdr[0]<<24|hdr[1]<<16|hdr[2]<<8|hdr[3]); memcpy(type,&hdr[4],4); }
        else { const struct png_chunk_attr *ch=(const struct png_chunk_attr*)hdr; length = ch->length; memcpy(type,ch->type,4); }
        if(length > (1u<<24)) { fprintf(stderr,"PNG chunk too big\n"); goto fail; }
        unsigned char *data = (unsigned char*)malloc(length); if(length && fread(data,1,length,f)!=length){ free(data); goto fail; }
    unsigned char crcbuf[4]; if(fread(crcbuf,1,4,f)!=4){ free(data); goto fail; }
    uint32_t crc_read = (uint32_t)(crcbuf[0]<<24|crcbuf[1]<<16|crcbuf[2]<<8|crcbuf[3]);
    // Compute CRC over chunk type then data per PNG spec.
    uint32_t c=0xFFFFFFFFu; if(!crc_init) make_crc_table();
        for(int i=0;i<4;i++) c = crc_table[(c ^ (unsigned char)type[i]) & 0xFF] ^ (c>>8);
        for(uint32_t i=0;i<length;i++) c = crc_table[(c ^ data[i]) & 0xFF] ^ (c>>8);
    uint32_t crc_calc = c ^ 0xFFFFFFFFu;
    if(crc_read != crc_calc){ fprintf(stderr,"[png] CRC mismatch in %s (got %08x expected %08x)\n", type, crc_read, crc_calc); crc_mismatch_count++; }
        if(!strcmp(type,"IHDR")){
            if(length!=13){ free(data); goto fail; }
            if(manual){ struct png_ihdr_manual ih; memcpy(&ih,data,13); w=mz_bswap32(ih.width); h=mz_bswap32(ih.height); bit_depth=ih.bit_depth; color_type=ih.color_type; }
            else { const struct png_ihdr_attr *ih=(const struct png_ihdr_attr*)data; w=ih->width; h=ih->height; bit_depth=ih->bit_depth; color_type=ih->color_type; }
            gotIHDR=1; interlace = data[12]; if(bit_depth!=8){ fprintf(stderr,"PNG only 8-bit supported\n"); free(data); goto fail; }
            if(interlace){ /* Adam7 handled later */ }
        } else if(!strcmp(type,"IDAT")){
            if(compSize+length > compCap){ size_t nc = (compCap?compCap*2:65536); while(nc < compSize+length) nc*=2; unsigned char *tmp=(unsigned char*)realloc(compressed,nc); if(!tmp){ free(data); goto fail; } compressed=tmp; compCap=nc; }
            memcpy(compressed+compSize,data,length); compSize+=length;
        } else if(!strcmp(type,"PLTE")){
            if(length % 3 || length/3 > 256){ fprintf(stderr,"PNG invalid PLTE length\n"); free(data); goto fail; }
            free(palette); palette=(unsigned char*)malloc(length); memcpy(palette,data,length); palette_entries=length/3;
        } else if(!strcmp(type,"tRNS")){
            free(trns); trns=(unsigned char*)malloc(length); memcpy(trns,data,length); trns_len=length; // Interpretation depends on color type; apply during expansion.
        } else if(!strcmp(type,"gAMA")){
            if(length==4){ uint32_t g = (data[0]<<24)|(data[1]<<16)|(data[2]<<8)|data[3]; if(g) { gamma_val = g / 100000.0; have_gamma=1; } }
        } else if(!strcmp(type,"sRGB")){
            if(length==1){ srgb_intent = data[0]; }
        } else if(!strcmp(type,"pHYs")){
            if(length==9){ phys_ppux = (data[0]<<24)|(data[1]<<16)|(data[2]<<8)|data[3]; phys_ppuy = (data[4]<<24)|(data[5]<<16)|(data[6]<<8)|data[7]; phys_unit = data[8]; have_phys=1; }
        } else if(!strcmp(type,"iCCP")){
            // profile_name\0 compression_method compressed_profile...
            size_t i=0; while(i<length && data[i] && i<79) i++; /* name */
        } else if(!strcmp(type,"IEND")){
            free(data); break;
        } else {
            // Ancillary chunk: log & skip
            int critical = isupper((unsigned char)type[0]);
            if(critical){ fprintf(stderr,"[png] Unknown critical chunk %s\n", type); free(data); goto fail; }
        }
        free(data);
    }
    if(!gotIHDR){ goto fail; }
    out->format=IMG_PNG; out->width=w; out->height=h;
    if(!decode_pixels){ free(compressed); return 0; }
    // Decompress
    z_stream zs; memset(&zs,0,sizeof zs); if(inflateInit(&zs)!=Z_OK){ goto fail; }
    zs.next_in=compressed; zs.avail_in=(uInt)compSize;
    unsigned bpp_raw;
    switch(color_type){
        case 0: bpp_raw=1; break; // Gray
        case 2: bpp_raw=3; break; // RGB
        case 3: bpp_raw=1; break; // Indexed
        case 4: bpp_raw=2; break; // Gray+Alpha
        case 6: bpp_raw=4; break; // RGBA
        default: fprintf(stderr,"PNG unsupported color type %d\n", color_type); goto fail;
    }
    unsigned bpp = bpp_raw;
    unsigned char *pixels=(unsigned char*)malloc((size_t)w*h*4); if(!pixels){ inflateEnd(&zs); goto fail; }
    if(!interlace){
        size_t rowBytes = 1 + w * bpp_raw; size_t rawSize = rowBytes * h;
        unsigned char *raw = (unsigned char*)malloc(rawSize); if(!raw){ inflateEnd(&zs); goto fail; }
        zs.next_out=raw; zs.avail_out=(uInt)rawSize; int infret=inflate(&zs,Z_FINISH); if(infret!=Z_STREAM_END){ free(raw); inflateEnd(&zs); goto fail; }
        inflateEnd(&zs); free(compressed); compressed=NULL;
        for(uint32_t y=0;y<h;y++){
            unsigned char *row = raw + y*rowBytes; unsigned ftype = row[0]; unsigned char *px = row+1;
            for(uint32_t x=0;x<w*bpp;x++){
                unsigned char left = (x>=bpp)? px[x-bpp]:0;
                unsigned char up = (y? raw[(y-1)*rowBytes + 1 + x] : 0);
                unsigned char up_left = (y && x>=bpp)? raw[(y-1)*rowBytes + 1 + x - bpp]:0;
                switch(ftype){ case 0: break; case 1: px[x]+=left; break; case 2: px[x]+=up; break; case 3: px[x]=(unsigned char)(px[x]+(uint8_t)((left+up)/2)); break; case 4:{ int p=left+up-up_left; int pa=abs(p-left), pb=abs(p-up), pc=abs(p-up_left); unsigned char pr=(pa<=pb&&pa<=pc)?left:(pb<=pc?up:up_left); px[x]+=pr; } break; default: break; }
            }
            for(uint32_t x=0;x<w;x++){
                size_t di = ((size_t)y*w + x)*4; if(color_type==2){ size_t srci=x*3; pixels[di+0]=px[srci]; pixels[di+1]=px[srci+1]; pixels[di+2]=px[srci+2]; pixels[di+3]=255; }
                else if(color_type==6){ size_t srci=x*4; pixels[di+0]=px[srci]; pixels[di+1]=px[srci+1]; pixels[di+2]=px[srci+2]; pixels[di+3]=px[srci+3]; }
                else if(color_type==0){ unsigned v=px[x]; pixels[di+0]=pixels[di+1]=pixels[di+2]=v; pixels[di+3]=(trns_len>=2 && px[x]==trns[1]?0:255); }
                else if(color_type==4){ size_t srci=x*2; unsigned v=px[srci]; pixels[di+0]=pixels[di+1]=pixels[di+2]=v; pixels[di+3]=px[srci+1]; }
                else if(color_type==3){ unsigned idx=px[x]; if(idx>=palette_entries){ pixels[di+0]=pixels[di+1]=pixels[di+2]=0; pixels[di+3]=255; } else { pixels[di+0]=palette[idx*3]; pixels[di+1]=palette[idx*3+1]; pixels[di+2]=palette[idx*3+2]; pixels[di+3]=(trns && idx<trns_len)? trns[idx]:255; } }
                else { pixels[di+0]=pixels[di+1]=pixels[di+2]=0; pixels[di+3]=255; }
            }
        }
        free(raw);
    } else {
        // Adam7 interlace
        static const int pass_starts_x[7]={0,4,0,2,0,1,0};
        static const int pass_starts_y[7]={0,0,4,0,2,0,1};
        static const int pass_step_x[7]={8,8,4,4,2,2,1};
        static const int pass_step_y[7]={8,8,8,4,4,2,2};
        // Precompute total decompressed size
        size_t total=0; uint32_t pass_w[7], pass_h[7];
        for(int p=0;p<7;p++){ uint32_t pw=0, ph=0; if(w>pass_starts_x[p]) pw = (w - pass_starts_x[p] + pass_step_x[p]-1)/pass_step_x[p]; if(h>pass_starts_y[p]) ph = (h - pass_starts_y[p] + pass_step_y[p]-1)/pass_step_y[p]; pass_w[p]=pw; pass_h[p]=ph; if(pw && ph) total += (size_t)ph * (1 + pw * bpp_raw); }
        unsigned char *raw = (unsigned char*)malloc(total); if(!raw){ inflateEnd(&zs); goto fail; }
        zs.next_out=raw; zs.avail_out=(uInt)total; int infret=inflate(&zs,Z_FINISH); if(infret!=Z_STREAM_END){ free(raw); inflateEnd(&zs); goto fail; }
        inflateEnd(&zs); free(compressed); compressed=NULL;
        size_t cursor=0;
        for(int p=0;p<7;p++){
            uint32_t pw=pass_w[p], ph=pass_h[p]; if(!pw||!ph) continue; size_t rowBytes=1+pw*bpp_raw;
            for(uint32_t ry=0; ry<ph; ry++){
                unsigned char *row = raw + cursor + ry*rowBytes; unsigned ftype=row[0]; unsigned char *px=row+1;
                for(uint32_t x=0;x<pw*bpp;x++){
                    unsigned char left = (x>=bpp)? px[x-bpp]:0;
                    unsigned char up = (ry? raw[cursor + (ry-1)*rowBytes + 1 + x] : 0);
                    unsigned char up_left = (ry && x>=bpp)? raw[cursor + (ry-1)*rowBytes + 1 + x - bpp]:0;
                    switch(ftype){ case 0: break; case 1: px[x]+=left; break; case 2: px[x]+=up; break; case 3: px[x]=(unsigned char)(px[x]+(uint8_t)((left+up)/2)); break; case 4:{ int pg=left+up-up_left; int pa=abs(pg-left), pb=abs(pg-up), pc=abs(pg-up_left); unsigned char pr=(pa<=pb&&pa<=pc)?left:(pb<=pc?up:up_left); px[x]+=pr; } break; default: break; }
                }
                // Map pass pixels to final image
                uint32_t y_real = pass_starts_y[p] + ry*pass_step_y[p]; if(y_real>=h) continue;
                for(uint32_t rx=0; rx<pw; rx++){
                    uint32_t x_real = pass_starts_x[p] + rx*pass_step_x[p]; if(x_real>=w) continue; size_t di=((size_t)y_real*w + x_real)*4;
                    if(color_type==2){ size_t srci=rx*3; pixels[di+0]=px[srci]; pixels[di+1]=px[srci+1]; pixels[di+2]=px[srci+2]; pixels[di+3]=255; }
                    else if(color_type==6){ size_t srci=rx*4; pixels[di+0]=px[srci]; pixels[di+1]=px[srci+1]; pixels[di+2]=px[srci+2]; pixels[di+3]=px[srci+3]; }
                    else if(color_type==0){ unsigned v=px[rx]; pixels[di+0]=pixels[di+1]=pixels[di+2]=v; pixels[di+3]=(trns_len>=2 && px[rx]==trns[1]?0:255); }
                    else if(color_type==4){ size_t srci=rx*2; unsigned v=px[srci]; pixels[di+0]=pixels[di+1]=pixels[di+2]=v; pixels[di+3]=px[srci+1]; }
                    else if(color_type==3){ unsigned idx=px[rx]; if(idx>=palette_entries){ pixels[di+0]=pixels[di+1]=pixels[di+2]=0; pixels[di+3]=255; } else { pixels[di+0]=palette[idx*3]; pixels[di+1]=palette[idx*3+1]; pixels[di+2]=palette[idx*3+2]; pixels[di+3]=(trns && idx<trns_len)? trns[idx]:255; } }
                    else { pixels[di+0]=pixels[di+1]=pixels[di+2]=0; pixels[di+3]=255; }
                }
            }
            cursor += (size_t)ph * rowBytes;
        }
        free(raw);
    }
    out->pixels=pixels; out->channels=4;
    if(getenv("SSO_PNG_META")){
        fprintf(stderr,"[png-meta] w=%u h=%u interlace=%d gamma=%s srgb_intent=%d phys=%s crc_mismatches=%u idat_chunks=%u palette=%u trns=%u adam7=%d\n",
                w,h,interlace,
                have_gamma?"yes":"no", srgb_intent, have_phys?"yes":"no", crc_mismatch_count,
                idat_count, palette_entries, trns_len>0, interlace);
    }
    return 0;
fail:
    free(compressed); free(palette); free(trns); return -1;
}

int load_png(const char *path, int decode_pixels, int manual, ImageData *out){
    memset(out,0,sizeof *out); FILE *f=fopen(path,"rb"); if(!f){ perror("fopen"); return -1; }
    int rc = load_png_stream(f,manual,decode_pixels,out); if(!rc){ out->format=IMG_PNG; }
    fclose(f); return rc;
}
