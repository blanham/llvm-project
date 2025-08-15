#include "image_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void image_free(ImageData *img) {
    if(!img) return; free(img->pixels); memset(img,0,sizeof *img); }

static ImageFormat sniff(const unsigned char *h, size_t n){
    if(n>=2 && h[0]=='B'&&h[1]=='M') return IMG_BMP;
    if(n>=8 && h[0]==137&&h[1]==80&&h[2]==78&&h[3]==71) return IMG_PNG;
    if(n>=2 && h[0]==0xFF && h[1]==0xD8) return IMG_JPEG;
    if(n>=4 && !memcmp(h,"qoif",4)) return IMG_QOI;
    return IMG_UNKNOWN;
}

int load_image_any(const char *path, int decode_pixels, int manual, ImageData *out){
    FILE *f=fopen(path,"rb"); if(!f){ perror("fopen"); return -1; }
    unsigned char head[32]; size_t n=fread(head,1,sizeof head,f); fclose(f);
    ImageFormat fmt = sniff(head,n); int rc=-1;
    switch(fmt){
        case IMG_BMP: rc=load_bmp(path,decode_pixels,manual,out); break;
        case IMG_PNG: rc=load_png(path,decode_pixels,manual,out); break;
        case IMG_JPEG: rc=load_jpeg(path,decode_pixels,manual,out); break;
        case IMG_QOI: rc=load_qoi(path,decode_pixels,manual,out); break;
        default: fprintf(stderr,"Unknown format: %s\n", path); return -1;
    }
    return rc;
}
