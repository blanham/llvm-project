// Temporary demo: Minimal PPM (P6) image loader using scalar_storage_order to test endian fields.
// Not for upstream. Demonstrates mixing manual and attribute-based endian handling and memcpy.
// Build: clang -O2 -Wall -Wextra -std=c11 image_viewer/main.c -o image_viewer
// Usage: ./image_viewer <file.ppm> [--manual]
// Writes per-channel average to stdout. PPM header numbers are ASCII (big-endian representation not relevant),
// so we fabricate a small binary header struct scenario to exercise the attribute along with pixel parsing.

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

// We'll define a made-up binary footer appended to the PPM containing big-endian 32-bit width & height duplicates
// to exercise attribute loads via a single memcpy of the footer block.
// Footer layout (after pixel data): magic 4 bytes "FTR0", be32 width_dup, be32 height_dup

typedef uint32_t __attribute__((scalar_storage_order("big-endian"))) be32;
struct ppm_footer_be_attr { char magic[4]; be32 width_dup; be32 height_dup; };
struct ppm_footer_manual { char magic[4]; uint32_t width_dup; uint32_t height_dup; };

static inline uint32_t bswap32(uint32_t v){ return __builtin_bswap32(v); }

static int skip_ws_comments(FILE *f){
    int c; do { c=fgetc(f); if(c=='#'){ while(c!='\n' && c!=EOF) c=fgetc(f); } } while(isspace(c));
    if(c!=EOF) ungetc(c,f); return 0;
}

static int parse_uint(FILE *f, unsigned *out){
    skip_ws_comments(f); int c=fgetc(f); if(!isdigit(c)) return -1; unsigned v=0; while(isdigit(c)){ v = v*10 + (unsigned)(c-'0'); c=fgetc(f);} if(c!=EOF) ungetc(c,f); *out=v; return 0; }

static int load_ppm(const char *path, int manual){
    FILE *f=fopen(path,"rb"); if(!f){ perror("fopen"); return -1; }
    char magic[3]={0}; if(fread(magic,1,2,f)!=2){ fclose(f); return -1; }
    if(strcmp(magic,"P6")){ fprintf(stderr,"Not P6 PPM\n"); fclose(f); return -1; }
    unsigned w,h,maxv; if(parse_uint(f,&w)||parse_uint(f,&h)||parse_uint(f,&maxv)){ fprintf(stderr,"Header parse fail\n"); fclose(f); return -1; }
    int c=fgetc(f); if(c!='\n'){ if(c==EOF){ fclose(f); return -1; } }
    size_t pix_sz = (size_t)w*h*3; unsigned char *pixels = malloc(pix_sz); if(!pixels){ fclose(f); return -1; }
    if(fread(pixels,1,pix_sz,f)!=pix_sz){ fprintf(stderr,"Pixel read short\n"); free(pixels); fclose(f); return -1; }

    // Try reading footer if present.
    unsigned char footer_buf[sizeof(struct ppm_footer_manual)];
    size_t footer_read = fread(footer_buf,1,sizeof footer_buf,f);
    int have_footer = footer_read == sizeof footer_buf && memcmp(footer_buf,"FTR0",4)==0;
    unsigned footer_w = 0, footer_h = 0;
    if (have_footer) {
        if (manual) {
            struct ppm_footer_manual fm; memcpy(&fm, footer_buf, sizeof fm);
            footer_w = bswap32(fm.width_dup); footer_h = bswap32(fm.height_dup); // stored big-endian
        } else {
            const struct ppm_footer_be_attr *fa = (const struct ppm_footer_be_attr*)footer_buf;
            footer_w = fa->width_dup; footer_h = fa->height_dup; // auto swap
        }
    }

    // Compute average color.
    unsigned long long sum_r=0,sum_g=0,sum_b=0; for(size_t i=0;i<pix_sz;i+=3){ sum_r+=pixels[i]; sum_g+=pixels[i+1]; sum_b+=pixels[i+2]; }
    double scale = 1.0 / (double)(w*h);
    double avg_r = sum_r*scale, avg_g=sum_g*scale, avg_b=sum_b*scale;
    printf("Image %ux%u max=%u avgRGB=(%.2f,%.2f,%.2f)", w,h,maxv, avg_r, avg_g, avg_b);
    if(have_footer) printf(" footer_dup=%ux%u", footer_w, footer_h);
    printf(manual?" (manual)\n":" (attr)\n");

    free(pixels); fclose(f); return 0;
}

int main(int argc, char **argv){
    if(argc<2){ fprintf(stderr,"Usage: %s <file.ppm> [--manual]\n", argv[0]); return 1; }
    int manual = argc>2 && strcmp(argv[2],"--manual")==0;
    return load_ppm(argv[1], manual);
}
