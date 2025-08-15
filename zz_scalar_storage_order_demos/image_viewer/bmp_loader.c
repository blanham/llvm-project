// Full BMP parser (baseline subset) supporting uncompressed 24/32bpp.
// Demonstrates attributed endian fields.
#include "image_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma pack(push,1)
struct bmp_file_header_manual { uint16_t bfType; uint32_t bfSize; uint16_t r1; uint16_t r2; uint32_t offBits; };
struct bmp_info_header_manual { uint32_t biSize; int32_t biWidth; int32_t biHeight; uint16_t biPlanes; uint16_t biBitCount; uint32_t biCompression; uint32_t biSizeImage; int32_t biXPelsPerMeter; int32_t biYPelsPerMeter; uint32_t biClrUsed; uint32_t biClrImportant; };

struct bmp_file_header_attr { be16 bfType; be32 bfSize; be16 r1; be16 r2; be32 offBits; };
struct bmp_info_header_attr { be32 biSize; int32_t biWidth; int32_t biHeight; be16 biPlanes; be16 biBitCount; be32 biCompression; be32 biSizeImage; int32_t biXPelsPerMeter; int32_t biYPelsPerMeter; be32 biClrUsed; be32 biClrImportant; };
#pragma pack(pop)

static int read_all(FILE *f, void *buf, size_t n){ return fread(buf,1,n,f)==n?0:-1; }

int load_bmp(const char *path, int decode_pixels, int manual, ImageData *out){
    memset(out,0,sizeof *out); out->format=IMG_BMP;
    FILE *f=fopen(path,"rb"); if(!f){ perror("fopen"); return -1; }
    unsigned char hdr[14+40]; if(read_all(f,hdr,sizeof hdr)){ fclose(f); return -1; }
    uint32_t width=0,height=0; uint16_t bpp=0; uint32_t compression=0; uint32_t offBits=0; int32_t signedHeight=0;
    if(manual){
        struct bmp_file_header_manual fh; memcpy(&fh,hdr,14);
        if(fh.bfType!=0x4D42){ fclose(f); return -1; }
        struct bmp_info_header_manual ih; memcpy(&ih,hdr+14,40);
        width=ih.biWidth; signedHeight=ih.biHeight; height = signedHeight<0?-signedHeight:signedHeight; bpp=ih.biBitCount; compression=ih.biCompression; offBits=fh.offBits;
    } else {
        const struct bmp_file_header_attr *fh=(const struct bmp_file_header_attr*)hdr;
        if(fh->bfType!=0x4D42){ fclose(f); return -1; }
        const struct bmp_info_header_attr *ih=(const struct bmp_info_header_attr*)(hdr+14);
        width=ih->biWidth; signedHeight=ih->biHeight; height = signedHeight<0?-signedHeight:signedHeight; bpp=ih->biBitCount; compression=ih->biCompression; offBits=fh->offBits;
    }
    if(compression!=0){ fprintf(stderr,"BMP compression unsupported\n"); fclose(f); return -1; }
    if(bpp!=24 && bpp!=32){ fprintf(stderr,"BMP bpp %u unsupported\n", bpp); fclose(f); return -1; }
    if(!decode_pixels){ out->width=width; out->height=height; out->channels= (bpp==24?3:4); fclose(f); return 0; }
    if(fseek(f, offBits, SEEK_SET)){ fclose(f); return -1; }
    size_t rowStrideSrc = ((width * (bpp/8) +3) & ~3u); // padded to 4
    size_t bytes = rowStrideSrc * height;
    unsigned char *raw = (unsigned char*)malloc(bytes); if(!raw){ fclose(f); return -1; }
    if(read_all(f, raw, bytes)){ free(raw); fclose(f); return -1; }
    out->width=width; out->height=height; out->channels=4; out->pixels=(uint8_t*)malloc((size_t)width*height*4);
    if(!out->pixels){ free(raw); fclose(f); return -1; }
    int flip = signedHeight>0; // bottom-up if positive
    for(uint32_t y=0;y<height;y++){
        uint32_t sy = flip ? (height-1-y) : y;
        unsigned char *src = raw + sy*rowStrideSrc;
        for(uint32_t x=0;x<width;x++){
            unsigned char b=src[x*(bpp/8)+0], g=src[x*(bpp/8)+1], r=src[x*(bpp/8)+2];
            unsigned char a = (bpp==32)? src[x*(bpp/8)+3] : 255;
            size_t di = ( (size_t)y*width + x)*4;
            out->pixels[di+0]=r; out->pixels[di+1]=g; out->pixels[di+2]=b; out->pixels[di+3]=a;
        }
    }
    free(raw); fclose(f); return 0;
}
