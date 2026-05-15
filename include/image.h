#ifndef _IMAGE_H
#define _IMAGE_H

#include <stdlib.h>
#include <page.h>
#include <string.h>
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC(sz)           kmalloc(sz)
#define STBI_REALLOC(p,newsz)     krealloc(p, newsz)
#define STBI_FREE(p)              kfree(p)
#define STBI_ASSERT(x) if(!(x)) panic("STB_IMAGE ASSERT")
#define STBI_NO_STDIO
#define STBI_NO_SIMD
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_ONLY_BMP
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_LDR_TO_HDR(r,g,b) {0,0,0}
#define STBI_STB_IMAGE_STB_IMAGE_H
#define float int
#define double int

#include <stb_image.h>

#endif
