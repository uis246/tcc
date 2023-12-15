#include "png.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>

struct mappedpng map(const char *path) {
	struct mappedpng png = {0};
	bool ok;

	printf("Info: opening %s\n", path);
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

	png.colorType = png_get_color_type(png.ptr, png.info);
	png.bitDepth = png_get_bit_depth(png.ptr, png.info);
	ok = (png.colorType == PNG_COLOR_TYPE_RGBA || png.colorType == PNG_COLOR_TYPE_RGB) && png.bitDepth == 8;
	ok |= png.colorType == PNG_COLOR_TYPE_PALETTE && png.bitDepth == 8;
	if(!ok) {
		fprintf(stderr, "This color type(%" PRIu8 ") or depth(%" PRIu8 ") is not supported, but used in %s\n", png.colorType, png.bitDepth, path);
		goto fail;
	}

	png.x = png_get_image_width(png.ptr, png.info);
	png.y = png_get_image_height(png.ptr, png.info);

	if(png.colorType == PNG_COLOR_TYPE_PALETTE) {
		png_uint_32 plt = png_get_PLTE(png.ptr, png.info, &png.paletted.plt, &png.paletted.numcolors);
		plt = png_get_tRNS(png.ptr, png.info, &png.paletted.alpha, &png.paletted.numtransparent, NULL);
		if(plt == 0) {
			fprintf(stderr, "Image %s has palette color type, but no transparency\n", path);
			goto fail;
		}
	}

	int unit_type;
	if(png_get_oFFs(png.ptr, png.info, &png.offset.x, &png.offset.y, &unit_type) == PNG_INFO_oFFs)
		switch(unit_type) {
			case PNG_OFFSET_PIXEL:
				//Yay! Offset!
				//png.offseted = true;
				break;
			case PNG_OFFSET_MICROMETER:
				fprintf(stderr, "Image %s has set offset in micrometers instead of pixels\n", path);
				goto fail;
			default:
				fprintf(stderr, "Image %s has some bullshit offset type\n", path);
				goto fail;
		}
	else {
		png.offset.x = 0;
		png.offset.y = 0;
	}
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

void unmap(struct mappedpng *png) {
	png_read_end(png->ptr, png->info);
	png_destroy_read_struct(&png->ptr, &png->info, NULL);
	if(png->f)
		fclose(png->f);
}


void mapwrite(const char *path, struct mappedpng *png) {
	png->f = fopen(path, "wb");
	if(!png->f) {
		fprintf(stderr, "Failed to open for write \"%s\"", path);
		goto fail;
	}

	png->write = true;

	png->ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if(!png->ptr) {
		fprintf(stderr, "Failed to create write context\n");
		goto fail;
	}

	png->info = png_create_info_struct(png->ptr);
	if(!png->info) {
		fprintf(stderr, "Failed to create write context\n");
		goto fail;
	}

	if(setjmp(png_jmpbuf(png->ptr))) {
		fprintf(stderr, "Failed to write\n");
		goto fail;
	}

	png_init_io(png->ptr, png->f);
	png_set_sig_bytes(png->ptr, 0);

	png_set_IHDR(png->ptr, png->info, png->x, png->y,
		8, png->colorType, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	if(png->offset.x != 0 || png->offset.y != 0)
		png_set_oFFs(png->ptr, png->info, png->offset.x, png->offset.y, PNG_OFFSET_PIXEL);
	if(png->colorType == PNG_COLOR_TYPE_PALETTE) {
		//Write palette
		assert(png->paletted.plt);
		png_set_PLTE(png->ptr, png->info, png->paletted.plt, png->paletted.numcolors);
		assert(png->paletted.alpha);
		png_set_tRNS(png->ptr, png->info, png->paletted.alpha, png->paletted.numtransparent, 0);
	}

	png_write_info(png->ptr, png->info);
	printf("Info: %s opened\n", path);
	return;

	fail:
	png_destroy_write_struct(&png->ptr, &png->info);
	if(png->f) {
		fclose(png->f);
		png->f = NULL;
	}
	png->ptr = NULL;
	abort();
	return;
}
void unmapwrite(struct mappedpng png) {
	printf("Written!\n");
	png_write_end(png.ptr, png.info);
	png_destroy_write_struct(&png.ptr, &png.info);
	if(png.f)
		fclose(png.f);
}
