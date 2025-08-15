// Common interfaces & types for image loaders (demo only; remove before upstream).
#ifndef ZZ_SSO_DEMOS_IMAGE_COMMON_H
#define ZZ_SSO_DEMOS_IMAGE_COMMON_H

#include <stdint.h>
#include <stddef.h>

#include "../common/attr_endian.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ImageFormat {
    IMG_UNKNOWN=0, IMG_BMP, IMG_PNG, IMG_JPEG, IMG_QOI
} ImageFormat;

typedef struct ImageData {
    ImageFormat format;
    uint32_t width;
    uint32_t height;
    uint32_t channels; // 3 or 4 for most
    uint8_t *pixels;   // RGBA8 (owned) or NULL on header-only parse
} ImageData;

void image_free(ImageData *img);

// Parse header only if decode_pixels==0, else load full pixels (where implemented).
int load_image_any(const char *path, int decode_pixels, int manual, ImageData *out);

// Individual loaders return 0 success, set out->format.
int load_bmp(const char *path, int decode_pixels, int manual, ImageData *out);
int load_png(const char *path, int decode_pixels, int manual, ImageData *out);
int load_jpeg(const char *path, int decode_pixels, int manual, ImageData *out);
int load_qoi(const char *path, int decode_pixels, int manual, ImageData *out);

#ifdef __cplusplus
}
#endif

#endif
