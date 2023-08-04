#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <assert.h>

#include <png.h>

#define PRE_LINE 1
#define POST_LINE 1
#define SCALE 8

struct mappedpng {
	FILE *f;
	png_structp ptr;
	png_infop info;
	png_bytepp rows;
	png_uint_32 x, y;
};

static struct mappedpng map(const char *path) {
	struct mappedpng png = {0};

	png.f = fopen(path, "rb");
	if(!png.f) {
		fprintf(stderr, "Failed to open %s\n", path);
		goto fail;
	}
	
	png.ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if(!png.ptr) {
		fprintf(stderr, "libpng error\n");
		goto fail;
	}

	png.info = png_create_info_struct(png.ptr);
	if(!png.info) {
		fprintf(stderr, "libpng error\n");
		goto fail;
	}

	if(setjmp(png_jmpbuf(png.ptr))) {
		fprintf(stderr, "Failed to read %s\n", path);
		goto fail;
	}

	png_init_io(png.ptr, png.f);
	png_read_info(png.ptr, png.info);

	if(png_get_color_type(png.ptr, png.info) != PNG_COLOR_TYPE_RGBA || png_get_bit_depth(png.ptr, png.info) != 8) {
		fprintf(stderr, "This color space or depth is not implemented, but used in %s\n", path);
		goto fail;
	}

	png.x = png_get_image_width(png.ptr, png.info);
	png.y = png_get_image_height(png.ptr, png.info);

	printf("Info: %s opened\n", path);
	return png;

	fail:
	png_destroy_read_struct(&png.ptr, &png.info, NULL);
	if(png.f) {
		fclose(png.f);
		png.f = NULL;
	}
	png.ptr = NULL;
	abort();
	return png;
}
static void unmap(struct mappedpng png) {
	png_destroy_read_struct(&png.ptr, &png.info, NULL);
	if(png.f)
		fclose(png.f);
}

static struct mappedpng mapwrite(const char *path, const struct mappedpng in) {
	struct mappedpng png = {0};
	png.f = fopen(path, "wb");
	if(!png.f) {
		fprintf(stderr, "Failed to open for write \"%s\"", path);
		goto fail;
	}

	png.ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if(!png.ptr) {
		fprintf(stderr, "Failed to create write context\n");
		goto fail;
	}

	png.info = png_create_info_struct(png.ptr);
	if(!png.info) {
		fprintf(stderr, "Failed to create write context\n");
		goto fail;
	}

	if(setjmp(png_jmpbuf(png.ptr))) {
		fprintf(stderr, "Failed to write\n");
		goto fail;
	}

	png_init_io(png.ptr, png.f);
	png_set_sig_bytes(png.ptr, 0);

	png_set_IHDR(png.ptr, png.info, in.x * (SCALE + POST_LINE + PRE_LINE), in.y * (SCALE + POST_LINE + PRE_LINE),
		8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_DEFAULT);

	png_write_info(png.ptr, png.info);
	printf("Info: %s opened\n", path);
	return png;

	fail:
	png_destroy_write_struct(&png.ptr, &png.info);
	if(png.f) {
		fclose(png.f);
		png.f = NULL;
	}
	png.ptr = NULL;
	abort();
	return png;
}
static void unmapwrite(struct mappedpng png) {
	printf("Written!\n");
	png_write_end(png.ptr, png.info);
	png_destroy_write_struct(&png.ptr, &png.info);
	if(png.f)
		fclose(png.f);
}

void addLines(png_bytep out, png_bytep up, png_uint_32 x) {
	//Do cross-platform endianess black magic
	uint8_t BLACKu8[4] = {0, 0, 0, 255};
	uint32_t BLACK = *(uint32_t*)&BLACKu8[0];
	for(png_uint_32 i = 0, o = 0; i < x; i++) {
		for(int j=0; j<PRE_LINE; j++)
			((uint32_t*)out)[o++] = BLACK;
		for(int j=0; j<SCALE; j++)
			((uint32_t*)out)[o++] = ((uint32_t*)up)[i];
		for(int j=0; j<POST_LINE; j++)
			((uint32_t*)out)[o++] = BLACK;
	}
}

void grid(struct mappedpng up, const char *outpath) {
	void *buf;
	struct mappedpng out = mapwrite(outpath, up);
	png_uint_32 rb = png_get_rowbytes(up.ptr, up.info);
	buf = malloc(rb * up.x * (1 + (SCALE + POST_LINE + PRE_LINE)*2));
	if(!buf) {
		fprintf(stderr, "Failed to allocate memory!\n");
		exit(-1);
	}
	void *outP, *upP, *black;
	upP = buf;
	outP = buf + rb;
	black = outP + rb*(SCALE + POST_LINE + PRE_LINE);

	//Do cross-platform endianess black magic
	uint8_t BLACKu8[4] = {0, 0, 0, 255};
	uint32_t BLACK = *(uint32_t*)&BLACKu8[0];
	for(png_uint_32 i = 0, sx = up.x * (SCALE + POST_LINE + PRE_LINE); i < sx; i++)
		((uint32_t*)black)[i] = BLACK;
	for(png_uint_32 i = 0; i < up.y; i++) {
		for(int i=0; i<PRE_LINE; i++)
			png_write_row(out.ptr, black);
		png_read_row(up.ptr, upP, NULL);
		addLines(outP, upP, up.x);
		for(int i=0; i<SCALE; i++)
			png_write_row(out.ptr, outP);
		for(int i=0; i<POST_LINE; i++)
			png_write_row(out.ptr, black);
	}
	unmapwrite(out);
	free(buf);
}

int main(int argc, char **argv) {
	char *out = "out.png";
	if(argc < 2 || argc > 3) {
		printf("Usage: %s INPUT [OUTPUT]\n", argv[0]);
		return 0;
	}
	if(argc == 3)
		out = argv[2];
	//up - upstream
	struct mappedpng up = map(argv[1]);
	grid(up, out);
	unmap(up);
}
