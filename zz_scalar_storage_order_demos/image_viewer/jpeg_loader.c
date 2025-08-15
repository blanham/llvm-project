// Baseline (non-progressive) JPEG decoder focused on demonstrating scalar_storage_order for big-endian
// segment length and dimension fields. Supports:
//   - Baseline SOF0, 8-bit precision
//   - 3 component YCbCr 4:4:4 sampling (all H=1,V=1)
//   - Standard Huffman tables (any provided in file)
//   - Quantization tables 8-bit precision
//   - Converts to 4-channel RGBA output
// Not (yet) supported: 4:2:0 / other sampling, restart markers, progressive JPEG, arithmetic coding.
// This is intentionally minimal & educational; DO NOT SHIP. Remove before upstream.
#include "image_common.h"
#include "../common/attr_endian.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int read_byte(FILE *f){ return fgetc(f); }
static int read_marker(FILE *f){
    int c; do { c = read_byte(f); if(c<0) return -1; } while(c!=0xFF);
    // skip fill FFs
    do { c = read_byte(f); if(c<0) return -1; } while(c==0xFF);
    return c;
}

struct seg_len { be16 len; }; // includes length bytes themselves (big-endian)

typedef struct { uint8_t code[256]; uint8_t size[256]; int16_t minCode[17]; int16_t maxCode[17]; int16_t valPtr[17]; uint8_t symbols[256]; int symbolCount; } HuffmanTable;
typedef struct { int16_t dc[64]; int16_t ac[64]; } DCTBlock; // placeholders

static void build_huffman(HuffmanTable *ht, const uint8_t *lengths, const uint8_t *symbols, int symCount){
    // lengths[1..16] number of codes of each length in bits.
    int k=0; for(int i=1;i<=16;i++) for(int j=0;j<lengths[i];j++) ht->symbols[k++] = symbols[k- (0)];
    ht->symbolCount = k;
    // Derive huff size & codes per Annex C.2
    int lastk=0; int code=0; for(int i=1;i<=16;i++){ for(int j=0;j<lengths[i];j++){ ht->size[lastk]=i; ht->code[lastk]=0; lastk++; } }
    int si = ht->size[0]; code=0; k=0; while(k<lastk){ ht->code[k]=code; code++; k++; if(k<lastk){ while(ht->size[k]!=si){ code <<=1; si++; } } }
    int p=0; for(int l=1;l<=16;l++){ int cnt=lengths[l]; if(!cnt){ ht->minCode[l]=-1; ht->maxCode[l]=-1; } else { ht->minCode[l]=ht->code[p]; ht->maxCode[l]=ht->code[p+cnt-1]; } ht->valPtr[l]=p; p+=cnt; }
}

typedef struct { const uint8_t *data; size_t size; size_t pos; int bit_buf; int bit_count; } BitStream;
static void bs_init(BitStream *bs,const uint8_t *data,size_t size){ bs->data=data; bs->size=size; bs->pos=0; bs->bit_buf=0; bs->bit_count=0; }
static int bs_fill(BitStream *bs){ while(bs->bit_count <= 16){ if(bs->pos>=bs->size) return -1; int b=bs->data[bs->pos++]; if(b==0xFF){ // skip stuffed zero
            if(bs->pos<bs->size && bs->data[bs->pos]==0x00) bs->pos++; else { /* marker encountered inside entropy stream (ignore here) */ }
        }
        bs->bit_buf = (bs->bit_buf<<8) | b; bs->bit_count +=8; }
    return 0; }
static int bs_get(BitStream *bs,int n,int *out){ if(bs_fill(bs)<0) return -1; int v = (bs->bit_buf >> (bs->bit_count - n)) & ((1<<n)-1); bs->bit_count -= n; *out=v; return 0; }
static int huff_decode(BitStream *bs, const HuffmanTable *ht, int *sym){ int code=0; for(int len=1; len<=16; len++){ int bit; if(bs_get(bs,1,&bit)<0) return -1; code = (code<<1)|bit; if(ht->minCode[len] != -1 && code <= ht->maxCode[len]){ int idx = ht->valPtr[len] + (code - ht->minCode[len]); *sym = ht->symbols[idx]; return 0; } }
    return -1; }

static const uint8_t zigzag[64] = { 0,1,5,6,14,15,27,28,2,4,7,13,16,26,29,42,3,8,12,17,25,30,41,43,9,11,18,24,31,40,44,53,10,19,23,32,39,45,52,54,20,22,33,38,46,51,55,60,21,34,37,47,50,56,59,61,35,36,48,49,57,58,62,63 };

static void idct_block(int16_t *blk){ // Very slow reference IDCT (not optimized)
    double tmp[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++){ double sum=0.0; for(int v=0; v<8; v++){ for(int u=0; u<8; u++){ double cu = (u==0)?0.70710678:1.0; double cv=(v==0)?0.70710678:1.0; double coeff = blk[v*8+u]; sum += cu*cv*coeff * cos(((2*x+1)*u*M_PI)/16.0) * cos(((2*y+1)*v*M_PI)/16.0); } } tmp[y*8+x] = sum/4.0; }
    for(int i=0;i<64;i++){ int val = (int)(tmp[i] + 128.5); if(val<0) val=0; if(val>255) val=255; blk[i]=(int16_t)val; }
}

typedef struct { uint8_t id; uint8_t h; uint8_t v; uint8_t tq; } Component;

int load_jpeg(const char *path, int decode_pixels, int manual, ImageData *out){
    (void)manual; memset(out,0,sizeof *out); out->format=IMG_JPEG;
    FILE *f=fopen(path,"rb"); if(!f){ perror("fopen"); return -1; }
    if(read_byte(f)!=0xFF || read_byte(f)!=0xD8){ fclose(f); return -1; }
    // Storage for tables
    uint8_t quant[4][64]; memset(quant,0,sizeof quant); int haveQuant[4]={0};
    HuffmanTable huffDC[4]={0}, huffAC[4]={0}; int haveHuffDC[4]={0}, haveHuffAC[4]={0};
    int width=0,height=0; Component comps[4]; int numComps=0; int sofParsed=0; size_t scanDataOff=0; size_t scanDataLen=0;
    // Parse markers until SOS
    while(1){ int m = read_marker(f); if(m<0){ fclose(f); return -1; }
        if(m==0xD9){ break; }
        if(m==0xDA){ // SOS
            struct seg_len L; if(fread(&L, sizeof L,1,f)!=1){ fclose(f); return -1; }
            int seglen=L.len; // big-endian auto-swapped
            if(seglen<2){ fclose(f); return -1; }
            int n = read_byte(f); if(n!=numComps){ // only support full-component scan
                fclose(f); return -1; }
            for(int i=0;i<n;i++){ int cid=read_byte(f); int tdta=read_byte(f); (void)cid; (void)tdta; }
            // Skip Ss Se Ah Al (baseline: Ss=0 Se=63 Ah=0 Al=0)
            read_byte(f); read_byte(f); read_byte(f);
            scanDataOff = ftell(f);
            // Read until next marker 0xFFxx (EOI or something) to get size
            // We'll slurp rest of file then trim trailing marker if found.
            fseek(f,0,SEEK_END); size_t fileEnd = ftell(f); fseek(f,scanDataOff,SEEK_SET);
            size_t alloc = fileEnd - scanDataOff; uint8_t *edata = (uint8_t*)malloc(alloc); if(!edata){ fclose(f); return -1; }
            size_t rd = fread(edata,1,alloc,f);
            // Trim trailing EOI if present
            if(rd>=2 && edata[rd-2]==0xFF && edata[rd-1]==0xD9){ rd-=2; }
            scanDataLen=rd;
            // If not decoding pixels we can stop now.
            if(!decode_pixels){ free(edata); break; }
            // Only support 3 components with 1x1 sampling (4:4:4)
            if(numComps!=3){ free(edata); fclose(f); return -1; }
            for(int i=0;i<numComps;i++){ if(comps[i].h!=1||comps[i].v!=1){ free(edata); fclose(f); fprintf(stderr,"JPEG sampling not supported (only 4:4:4)\n"); return -1; } }
            // Build entropy bitstream
            BitStream bs; bs_init(&bs, edata, rd);
            size_t mcuCountX = (width +7)/8; size_t mcuCountY = (height+7)/8;
            unsigned char *pixels = (unsigned char*)malloc((size_t)width*height*4); if(!pixels){ free(edata); fclose(f); return -1; }
            int16_t prevDC[3]={0,0,0};
            int qSel[3]; int dcSel[3]; int acSel[3]; for(int i=0;i<3;i++){ qSel[i]=comps[i].tq; dcSel[i]=0; acSel[i]=0; }
            for(size_t my=0; my<mcuCountY; my++){
                for(size_t mx=0; mx<mcuCountX; mx++){
                    int16_t block[3][64]; memset(block,0,sizeof block);
                    for(int ci=0;ci<3;ci++){
                        // Decode one 8x8 block
                        int sym; if(huff_decode(&bs, &huffDC[dcSel[ci]], &sym)<0){ /* error */ }
                        int sizeDC = sym; int dcDiff=0; if(sizeDC){ int bits; if(bs_get(&bs,sizeDC,&bits)<0){} if(bits < (1<<(sizeDC-1))) bits -= (1<<sizeDC)-1; dcDiff=bits; }
                        int dc = prevDC[ci] + dcDiff; prevDC[ci]=dc; block[ci][0]=dc;
                        int k=1; while(k<64){ if(huff_decode(&bs, &huffAC[acSel[ci]], &sym)<0) break; if(sym==0x00){ break; } // EOB
                            int run = sym>>4; int size = sym & 0xF; if(size==0){ if(run==0xF){ k+=16; continue; } }
                            k += run; if(k>=64) break; int bits; if(size){ if(bs_get(&bs,size,&bits)<0) break; if(bits < (1<<(size-1))) bits -= (1<<size)-1; block[ci][zigzag[k]] = bits; } k++; }
                        // Dequantize
                        if(haveQuant[qSel[ci]]) for(int i=0;i<64;i++) block[ci][i] = (int16_t)(block[ci][i] * quant[qSel[ci]][i]);
                        idct_block(block[ci]);
                    }
                    // Color convert and store 8x8 tile
                    for(int by=0; by<8; by++){
                        int py = (int)(my*8 + by); if(py>=height) break;
                        for(int bx=0; bx<8; bx++){
                            int px = (int)(mx*8 + bx); if(px>=width) break;
                            int Y = block[0][by*8+bx]; int Cb=block[1][by*8+bx]-128; int Cr=block[2][by*8+bx]-128;
                            int R = Y + (int)(1.402*Cr);
                            int G = Y - (int)(0.344136*Cb + 0.714136*Cr);
                            int B = Y + (int)(1.772*Cb);
                            if(R<0)R=0; if(R>255)R=255; if(G<0)G=0; if(G>255)G=255; if(B<0)B=0; if(B>255)B=255;
                            size_t di = ((size_t)py*width + px)*4; pixels[di+0]=(uint8_t)R; pixels[di+1]=(uint8_t)G; pixels[di+2]=(uint8_t)B; pixels[di+3]=255;
                        }
                    }
                }
            }
            free(edata); out->pixels=pixels; out->width=width; out->height=height; out->channels=4; break; // done
        } else if(m==0xC0){ // SOF0
            struct seg_len L; if(fread(&L,sizeof L,1,f)!=1){ fclose(f); return -1; }
            int seglen=L.len; if(seglen<8){ fclose(f); return -1; }
            int precision = read_byte(f); if(precision!=8){ fclose(f); return -1; }
            be16 h_be,w_be; if(fread(&h_be,2,1,f)!=1 || fread(&w_be,2,1,f)!=1){ fclose(f); return -1; }
            height = h_be; width = w_be; numComps = read_byte(f); if(numComps<=0||numComps>3){ fclose(f); return -1; }
            for(int i=0;i<numComps;i++){ comps[i].id=read_byte(f); int hv=read_byte(f); comps[i].h = (hv>>4)&0xF; comps[i].v = hv & 0xF; comps[i].tq=read_byte(f); }
            sofParsed=1;
        } else if(m==0xDB){ // DQT
            struct seg_len L; if(fread(&L,sizeof L,1,f)!=1){ fclose(f); return -1; }
            int seglen=L.len; int toRead = seglen - 2; while(toRead>0){ int pq_tq = read_byte(f); toRead--; int pq = (pq_tq>>4)&0xF; int tq = pq_tq & 0xF; if(pq!=0){ fclose(f); return -1; }
                if(tq>=4){ fclose(f); return -1; }
                for(int i=0;i<64;i++){ int v=read_byte(f); if(v<0){ fclose(f); return -1; } quant[tq][i]=(uint8_t)v; }
                haveQuant[tq]=1; toRead -= 64; }
        } else if(m==0xC4){ // DHT
            struct seg_len L; if(fread(&L,sizeof L,1,f)!=1){ fclose(f); return -1; }
            int seglen=L.len; int toRead = seglen - 2; while(toRead>0){ int tc_th = read_byte(f); toRead--; int tc=(tc_th>>4)&0xF; int th=tc_th&0xF; if(tc>1||th>3){ fclose(f); return -1; }
                uint8_t lengths[17]={0}; int total=0; for(int i=1;i<=16;i++){ int c=read_byte(f); if(c<0){ fclose(f); return -1; } lengths[i]= (uint8_t)c; total+=c; }
                uint8_t *symbols = (uint8_t*)malloc(total); if(!symbols){ fclose(f); return -1; }
                for(int i=0;i<total;i++){ int v=read_byte(f); if(v<0){ free(symbols); fclose(f); return -1; } symbols[i]=(uint8_t)v; }
                if(tc==0){ build_huffman(&huffDC[th], lengths, symbols, total); haveHuffDC[th]=1; } else { build_huffman(&huffAC[th], lengths, symbols, total); haveHuffAC[th]=1; }
                free(symbols); toRead -= (1+16+total); }
        } else { // Skip other markers
            struct seg_len L; if(fread(&L,sizeof L,1,f)!=1){ fclose(f); return -1; }
            int seglen=L.len; if(fseek(f, seglen-2, SEEK_CUR)){ fclose(f); return -1; }
        }
    }
    fclose(f);
    if(width<=0||height<=0){ return -1; }
    if(!decode_pixels){ out->width=width; out->height=height; out->channels=3; return 0; }
    if(!out->pixels){ // decode failed earlier
        return -1; }
    return 0;
}
