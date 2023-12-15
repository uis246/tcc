#include "common/png.h"

#include <iostream>
#include <cstdlib>
#include <vector>
#include <cstdint>
#include <cinttypes>
#include <span>
#include <set>
#include <algorithm>

typedef png_color color_t;

constexpr inline bool operator ==(const color_t &a, const color_t &b) {
	return a.red == b.red && a.green == b.green && a.blue == b.blue;
}

//Get palette-to-palette table
//Output plt color 0 is transparent
static std::vector<uint8_t> plt2pltTable(std::span<const color_t> input, std::span<const uint8_t> inputAlpha, std::vector<color_t> &outputPlt, bool allowGrowth) {
	size_t len = input.size(), alen = inputAlpha.size();
	std::vector<uint8_t> pltpair;
	bool alphaerr = false;
	for(size_t i = 0; i < len; i++) {
		if(i < alen && inputAlpha[i] != 255) {
			if(inputAlpha[i] != 0 && !alphaerr) {
				std::cerr << "Paletted image uses color with alpha neither 255 nor 0, marking as transparent" << std::endl;
				alphaerr = true;
			}
			pltpair.push_back(0);
		} else {
			color_t color = input[i];
			size_t index, outlen = outputPlt.size();
			for(index = 0; index < outlen && outputPlt[index] != color; index++);
			if(index == outlen) {
				if(!allowGrowth) {
					std::cerr << "Image wants color that does not exist in palette" << std::endl;
					exit(-1);
				}
				if(index == 255) {
					std::cerr << "Palette merge results in too big palette" << std::endl;
					//Oh shit
					exit(-1);
				}
				outputPlt.push_back(color);
			}
			pltpair.push_back(index + 1);
		}
	}

	return pltpair;
}

static png_bytep readPNG(mappedpng &png) {
	png_bytepp rows = (png_bytepp)alloca(png.y * sizeof(png_bytepp));
	size_t stride = png.colorType == PNG_COLOR_TYPE_RGB ? 3 : (png.colorType == PNG_COLOR_TYPE_PALETTE ? 1 : 4);
	png_bytep data = (png_bytep)malloc(png.x * png.y * stride);
	size_t offset = 0;
	for(png_uint_32 i = 0; i < png.y; i++) {
		rows[i] = data + offset;
		offset += png.x * stride;
	}
	png_read_image(png.ptr, rows);
	return data;
}

static void readPalette(const char *path, std::vector<color_t> &plt) {
	struct mappedpng png = map(path);
	if(png.colorType != PNG_COLOR_TYPE_RGB && png.colorType != PNG_COLOR_TYPE_RGBA) {
		std::cerr << "Cant read palette from image type " << png.colorType << std::endl;
		exit(-1);
	}
	//Read PNG
	if(png.x > 255 || png.x * png.y > 255) {
		std::cerr << "Palette image is too big" << std::endl;
		exit(-1);
	}
	png_bytep data = readPNG(png);
	//Clear just in case
	plt.clear();

	if(png.colorType == PNG_COLOR_TYPE_RGB) {
		//Start reading
		size_t offset = 0, size = png.x * png.y * 3;
		for(; offset < size; offset += 3) {
			const color_t color{data[offset], data[offset + 1], data[offset + 2]};
			//Add only unique
			if(std::find(plt.begin(), plt.end(), color) == plt.end())
				plt.push_back(color);
		}
	} else if(png.colorType == PNG_COLOR_TYPE_RGBA) {
		size_t offset = 0, size = png.x * png.y * 4;
		for(; offset < size; offset += 4) {
			const color_t color{data[offset], data[offset + 1], data[offset + 2]};
			const uint8_t alpha = data[offset + 3];
			//Skip transparent
			if(alpha == 0)
				continue;
			else if(alpha != 255) {
				std::cout << "Palette image has pixel with color != 255 and != 0, skipping" << std::endl;
				continue;
			}
			//Add only unique
			if(std::find(plt.begin(), plt.end(), color) == plt.end())
				plt.push_back(color);
		}
	} else {//paletted image
//		std::vector<color_t> palette;
		plt2pltTable(std::span<color_t>(png.paletted.plt, png.paletted.numcolors), std::span<const uint8_t>(png.paletted.alpha, png.paletted.numtransparent), plt, true);
		plt.insert(plt.begin(), {0, 0, 0});
	}

	free(data);
	unmap(&png);
}

void compile(int argc, const char * const *argv) {
	if(argc != 5)
		goto usage;
	{
	const char *output_file(argv[0]), *palette_file(argv[1]), *input_file(argv[2]);
	long offsetX, offsetY;
	{
		char *conv;
		offsetX = std::strtol(argv[3], &conv, 10);
		if(conv==argv[3])
			//Not a number
			goto usage;
		if(offsetX == LONG_MAX)
			//Too big number
			goto overrange;
		offsetY = std::strtol(argv[4], &conv, 10);
		if(conv==argv[4])
			//Not a number
			goto usage;
		if(offsetY == LONG_MAX)
			//Too big number
			goto overrange;
	}

	//Read palette
	//
	std::vector<color_t> palette;
	readPalette(palette_file, palette);

	//Read input, palletize and write output
	mappedpng input = map(input_file);
	png_bytep data;
	if(input.colorType == PNG_COLOR_TYPE_PALETTE) {
		std::vector<uint8_t> pltpair =
				plt2pltTable(std::span<color_t>(input.paletted.plt, input.paletted.numcolors), std::span<const uint8_t>(input.paletted.alpha, input.paletted.numtransparent), palette, false);
		data = readPNG(input);
		size_t size = input.x * input.y;
		for(size_t i = 0; i < size; i++)
			data[i] = pltpair[data[i]];
	} else if(input.colorType == PNG_COLOR_TYPE_RGBA) {
		png_bytep indata = readPNG(input);
		size_t offset = 0, size = input.x * input.y;
		data = (png_bytep)malloc(size);
		size *= 4;
		for(size_t i = 0; offset < size; offset += 4, i++) {
			const color_t color{indata[offset], indata[offset + 1], indata[offset + 2]};
			const uint8_t alpha = indata[offset + 3];
			if(alpha != 255) {
				if(alpha)
					std::cout << "Image has pixel with color != 255 and != 0, marking transparent" << std::endl;
				data[i] = 0;
				continue;
			}
			//Find color
			auto it = std::find(palette.begin(), palette.end(), color);
			if(it == palette.end()) {
				//Color not found in palette
				png_uint_32 x, y;
				y = offset / (input.x * 4);
				x = (offset % (input.x * 4)) / 4;
				//NOTE: how compiler should behave?
				std::cerr << "Out-of-palette color at x=" << x << " y=" << y << ", marking transparent" << std::endl;
				data[i] = 0;
			} else {
				data[i] = std::distance(palette.begin(), it) + 1;
			}
		}
		free(indata);
	} else if(input.colorType == PNG_COLOR_TYPE_RGB) {
		std::cerr << "Tried to compile pure RGB image. Is it a template?" << std::endl;
		exit(-1);
	}

	//Prepare output
	palette.insert(palette.begin(), {0, 0, 0});
	uint8_t zero = 0;
	mappedpng output;
	output.x = input.x;
	output.y = input.y;
	output.colorType = PNG_COLOR_TYPE_PALETTE;
	output.bitDepth = 8;
	output.offset.x = offsetX;
	output.offset.y = offsetY;
	output.write = true;
	output.paletted.alpha = &zero;
	output.paletted.numtransparent = 1;
	output.paletted.plt = palette.data();
	output.paletted.numcolors = palette.size();
	mapwrite(output_file, &output);

	//Write
	png_bytepp rows = (png_bytepp)alloca(output.y * sizeof(png_bytepp));
	for(png_uint_32 i = 0; i < output.y; i++)
		rows[i] = data + output.x * i;
	png_write_image(output.ptr, rows);

	//Free memory
	unmapwrite(output);
	unmap(&input);
	free(data);
	return;
	}

	usage:
	std::cout << "Compile tool usage: OUTPUT INPUT PALETTE OFFSETX OFFSETY\n";
	return;

	overrange:
	std::cout << "Offset is too big\n";
	return;
}

void link(int argc, char **argv) {
	usage:
	std::cout << "Link tool usage: OUTPUT INPUT1 INPUT2...\n";
	return;
}

int main(int argc, char **argv) {
	if(argc < 2) {
		std::cout << "Usage:\n\t-compile    Create paletted PNG with offset\n\t-link       Create big paletted PNG with offset from smaller ones\n";
		return -1;
	}

	std::string_view tool(argv[1]);
	if(tool == "-compile")
		compile(argc-2, argv+2);
	else if(tool == "-link")
		link(argc-2, argv+2);
	else
		std::cout << "Tool " << tool << " not found" << std::endl;
	return 0;
}
