#include "common/png.h"

#include <iostream>
#include <cstdlib>
#include <vector>
#include <cstdint>
#include <cinttypes>
#include <memory.h>
#include <span>
#include <set>
#include <algorithm>

typedef png_color color_t;

constexpr inline bool operator ==(const color_t &a, const color_t &b) {
	return a.red == b.red && a.green == b.green && a.blue == b.blue;
}

constexpr inline v2i32 operator +(const v2i32 &a, const v2i32 &b) {
	return {a.x + b.x, a.y + b.y};
}

constexpr inline v2i32 operator -(const v2i32 &a, const v2i32 &b) {
	return {a.x - b.x, a.y - b.y};
}

constexpr inline bool operator <(const v2i32 &a, const v2i32 &b) {
	return a.x < b.x && a.y < b.y;
}
constexpr inline bool operator >(const v2i32 &a, const v2i32 &b) {
	return a.x > b.x && a.y > b.y;
}

constexpr inline v2i32 minel(const v2i32 &a, const v2i32 &b) {
	return {a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y};
}
constexpr inline v2i32 maxel(const v2i32 &a, const v2i32 &b) {
	return {a.x > b.x ? a.x : b.x, a.y > b.y ? a.y : b.y};
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
	if(!data)
		abort();
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
	std::vector<color_t> palette;
	readPalette(palette_file, palette);

	//Read input, palletize and write output
	mappedpng input = map(input_file);
	png_bytep data = nullptr;
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
		if(!data)
			abort();
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
	std::cout << "Compile tool usage: OUTPUT INPUT PALETTE OFFSETX OFFSETY" << std::endl;
	return;

	overrange:
	std::cout << "Offset is too big" << std::endl;
	return;
}

struct activemapping {
	mappedpng *png;
	png_bytep pixels;
	v2i32 brc;
//	bool collides;
	constexpr bool operator <(const activemapping &b) const {
		return png->offset.x < b.png->offset.x;
	}
};

static constexpr bool operator <(const mappedpng &a, const mappedpng &b) {
	return a.offset.y < b.offset.y;
}

thread_local png_bytep out;

static void blendrow(const mappedpng &output, const std::span<const activemapping> ams) {
	//FIXME: current implementation assumes images will activate in same order they deactivate
	size_t active = ams.size(), xbase = 0;
	for(png_int_32 x = output.offset.x, xactive = 0;;) {
		size_t odist = x - output.offset.x;
		if(xactive == 0) {
			//Skip till next or end
			if(xbase == active) {
				//End line
				//Set rest to 0
				memset(out + odist, 0, output.offset.x + output.x - x);
				//Write line
				png_write_row(output.ptr, out);
				break;
			}
		}
		//Some pngs left
		if(ams[xbase + xactive].png->offset.x != x) {
			memset(out + odist, 0, ams[xbase + xactive].png->offset.x - x);
			x = ams[xbase + xactive].png->offset.x;
		}
		for(size_t i = xbase + xactive; i < active; i++)
			if(ams[i].png->offset.x == x) {
				xactive++;
			} else
				break;
		if(xactive == 1) {
			//Write as much as possible
			if(xbase + xactive == active || ams[xbase].brc.x <= ams[xbase + 1].png->offset.x) {
				//Write entire image
				std::copy(ams[xbase].pixels + ams[xbase].png->offset.x - x, ams[xbase].pixels + ams[xbase].png->x, out + odist);
				x = ams[xbase].brc.x;
				xactive--;
				xbase++;
			} else {
				//Write partial image
				std::copy(ams[xbase].pixels + ams[xbase].png->offset.x - x, ams[xbase].pixels + (ams[xbase + 1].png->offset.x - ams[xbase].png->offset.x), out + odist);
				x = ams[xbase + 1].png->offset.x;
			}
		} else {
			//TODO: not implemented
			std::cerr << "Overlapping PNGs are not implemented yet" << std::endl;
			exit(-ENOSYS);
			//Overwrite with last image for testing
			std::copy(ams[xbase + xactive - 1].pixels + ams[xbase + xactive - 1].png->offset.x - x, ams[xbase + xactive - 1].pixels + (ams[xbase + xactive - 1].png->x), out + odist);
			x = ams[xbase + xactive - 1].brc.x;
			for(size_t i = xbase; i < active; i++)
				if(ams[i].brc.x <= x) {
					xbase++;
					xactive--;
				} else
					break;
			//Write checking for conflicts
		}
	}
}

void link(int argc, char **argv) {
	if(argc < 3)
		goto usage;
	{
//	std::vector<char*> input_paths(argv + 1, argv + argc - 1);
	std::vector<mappedpng> inputs;
	std::vector<activemapping> ams;
	inputs.reserve(argc - 1);
	ams.reserve(argc - 1);
	v2i32 min(INT32_MAX, INT32_MAX), max(INT32_MIN, INT32_MIN);
	std::vector<color_t> wpalette;
	for(int i = 0; i < argc - 1; i++) {
		inputs.push_back(map(argv[i + 1]));
		mappedpng &png = inputs[i];
		if(png.colorType != PNG_COLOR_TYPE_PALETTE || png.paletted.numtransparent != 1 || png.paletted.alpha[0] != 0) {
			std::cerr << "File " << argv[i + 1] << " does not seems to be compiled template" << std::endl;
		}
		std::vector<color_t> palette(png.paletted.plt, png.paletted.plt + (size_t)png.paletted.numcolors);
		if(wpalette.empty()) {
			wpalette = std::move(palette);
		} else if(wpalette != palette) {
			std::cerr << "Palette mismatch, recompile all images" << std::endl;
			exit(-1);
		}
		min = minel(min, png.offset);
		v2i32 brc = png.offset + v2i32{(png_int_32)png.x, (png_int_32)png.y};
		max = maxel(max, brc);
	}
	std::sort(inputs.begin(), inputs.end());

	//Open write mapping
	mappedpng output;
	uint8_t zero = 0;
	output.x = max.x - min.x;
	output.y = max.y - min.y;
	output.colorType = PNG_COLOR_TYPE_PALETTE;
	output.bitDepth = 8;
	output.offset.x = min.x;
	output.offset.y = min.y;
	output.write = true;
	output.paletted.alpha = &zero;
	output.paletted.numtransparent = 1;
	output.paletted.plt = wpalette.data();
	output.paletted.numcolors = wpalette.size();
	mapwrite(argv[0], &output);
	out = (png_bytep)malloc(max.x - min.x);
	if(out == NULL) {
		std::cerr << "Out of memory" << std::endl;
		exit(-ENOMEM);
	}

	//FIXME: current implementation assumes images will activate in same order they deactivate
	for(png_int_32 y = min.y, base = 0, active = 0; y < max.y; y++) {
		bool dirty = false;
		//Activate all mappins
		for(png_int_32 i = base + active; i < argc - 1; i++)
			if(inputs[i].offset.y == y) {
				mappedpng &png = inputs[i];
				v2i32 brc = png.offset + v2i32{(png_int_32)png.x, (png_int_32)png.y};
				activemapping am{&png, (png_bytep)malloc(png.x), brc};
				if(am.pixels == NULL) {
					std::cerr << "Out of memory" << std::endl;
					exit(-ENOMEM);
				}
				ams.push_back(am);
				active++;
				dirty = true;
			} else
				break;
		if(dirty)
			std::sort(ams.begin(), ams.end());
		//Read rows
		for(size_t i = 0; i < (size_t)active; i++) {
			png_read_row(ams[i].png->ptr, ams[i].pixels, NULL);
		}
		//Write row
		blendrow(output, std::span<const activemapping>(ams));
		//Deactivate useless
		for(size_t i = 0; i < (size_t)active;) {
			if(ams[i].brc.y - 1 == y) {
				free(ams[i].pixels);
				unmap(ams[i].png);
				ams.erase(ams.begin() + i);
				active--;
			} else
				i++;
		}
	}
	free(out);
	unmapwrite(output);
	return;
	}

	usage:
	std::cout << "Link tool usage: OUTPUT INPUT1 INPUT2..." << std::endl;
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
