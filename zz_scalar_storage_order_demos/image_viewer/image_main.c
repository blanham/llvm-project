// Unified image demo main driver with benchmarking, manual vs attr paths, and optional pixel decode.
#include "image_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

static int use_manual=0; static size_t bench_iter=0; static int decode_pixels=0; static const char *dump_raw_path=NULL; static int do_view=0; static const char *ppm_out=NULL;

static double now_sec(void){ struct timeval tv; gettimeofday(&tv,NULL); return tv.tv_sec + tv.tv_usec/1e6; }

static const char *fmt_name(ImageFormat f){ switch(f){ case IMG_BMP: return "BMP"; case IMG_PNG: return "PNG"; case IMG_JPEG: return "JPEG"; case IMG_QOI: return "QOI"; default: return "?"; } }

int main(int argc,char**argv){ if(argc<2){ fprintf(stderr,"Usage: %s <image> [--manual] [--bench=ITER] [--decode] [--dump-raw=FILE] [--view] [--ppm=FILE]\n", argv[0]); return 1; }
    const char *path=argv[1];
    for(int i=2;i<argc;i++){
        if(!strcmp(argv[i],"--manual")) use_manual=1;
        else if(!strncmp(argv[i],"--bench=",8)) bench_iter=strtoull(argv[i]+8,NULL,10);
    else if(!strcmp(argv[i],"--decode")) decode_pixels=1;
    else if(!strncmp(argv[i],"--dump-raw=",11)){ dump_raw_path=argv[i]+11; decode_pixels=1; }
    else if(!strcmp(argv[i],"--view")){ do_view=1; decode_pixels=1; }
    else if(!strncmp(argv[i],"--ppm=",6)){ ppm_out=argv[i]+6; decode_pixels=1; }
    }
    if(!bench_iter){
        ImageData img; if(load_image_any(path, decode_pixels, use_manual, &img)){ return 1; }
    printf("%s %ux%u ch=%u %s\n", fmt_name(img.format), img.width, img.height, img.channels, use_manual?"manual":"attr");
        if(dump_raw_path && img.pixels && img.channels==4){ FILE *df=fopen(dump_raw_path,"wb"); if(!df){ perror("fopen dump_raw"); image_free(&img); return 1; } size_t n=(size_t)img.width*img.height*4; if(fwrite(img.pixels,1,n,df)!=n){ fprintf(stderr,"short write raw\n"); } fclose(df); }
        if(ppm_out && img.pixels){ FILE *pf=fopen(ppm_out,"wb"); if(!pf){ perror("fopen ppm"); image_free(&img); return 1; } fprintf(pf,"P6\n%u %u\n255\n", img.width, img.height); // drop alpha
            for(uint32_t y=0;y<img.height;y++){ for(uint32_t x=0;x<img.width;x++){ size_t di=((size_t)y*img.width+x)*4; fputc(img.pixels[di+0],pf); fputc(img.pixels[di+1],pf); fputc(img.pixels[di+2],pf); } }
            fclose(pf); }
        if(do_view && img.pixels){
            #include "minifb_stub.h"
            int win = mfb_open_ex("scalar_storage_order viewer", img.width, img.height, 0);
            if(win){
                // Simple one-frame update; loop not persistent to keep demo minimal
                mfb_update(win, img.pixels);
                mfb_close(win);
            } else if(!ppm_out){ fprintf(stderr,"MiniFB not available (compile with -DWITH_MINIFB), use --ppm=<file> for output.\n"); }
        }
        image_free(&img); return 0;
    }
    double s=now_sec(); for(size_t i=0;i<bench_iter;i++){ ImageData img; if(load_image_any(path, decode_pixels, use_manual, &img)){ return 1; } image_free(&img); } double e=now_sec();
    printf("BENCH image iterations=%zu total=%.3fs per=%.6fs mode=%s\n", bench_iter, e-s,(e-s)/bench_iter,use_manual?"manual":"attr");
    return 0; }
