#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int write_tiff(const char* outputpath, uint8_t* imgbytes, const uint32_t width, const uint32_t height, const int bpp);

#ifdef __cplusplus
};
#endif
