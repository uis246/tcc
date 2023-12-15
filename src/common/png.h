#pragma once

#include <png.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
struct v2i32 {
	png_int_32 x, y;
};

struct mappedpng {
	FILE *f;
	png_structp ptr;
	png_infop info;
	png_uint_32 x, y;//size
	struct v2i32 offset;
	struct {
		png_colorp plt;
		png_bytep alpha;
		int numcolors, numtransparent;
	} paletted;
	png_byte colorType, bitDepth;
	bool write/*, offseted*/;
};

struct mappedpng map(const char *path);
void unmap(struct mappedpng *png);
void mapwrite(const char *path, struct mappedpng *png);
void unmapwrite(struct mappedpng png);
#ifdef __cplusplus
}
#endif
