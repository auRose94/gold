#include <cstring>
#include "image.hpp"

#include <bx/allocator.h>
#include <bx/readerwriter.h>

#include "bimg/bimg.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <iterator>
#include <string_view>

namespace gold {
	using namespace std;

	bimg::ImageContainer* image::getContainer() {
		auto con = (bimg::ImageContainer*)getPtr("con");
		if (con) return con;

		auto format =
			(bimg::TextureFormat::Enum)getUInt32("format");
		auto width = getUInt16("width");
		auto height = getUInt16("height");
		auto depth = getUInt16("depth");
		auto numLayers = getUInt16("layers");
		auto cubeMap = getBool("cubeMap");
		auto mips = getUInt16("numMips") != 0;
		auto bin = getStringView("data");
		if (width == 0 || height == 0 || bin.empty()) return nullptr;

		con = bimg::imageAlloc(
			&defaultAllocator,
			format,
			width,
			height,
			depth,
			numLayers,
			cubeMap,
			mips,
			bin.data());
		if (con) {
			setPtr("con", con);
			return con;
		}

		return nullptr;
	}

	image::image() : object() {}
	image::image(object config) : object(config) {
		auto con = bimg::ImageContainer();
		con.m_allocator = &gold::defaultAllocator;
		auto view = getStringView("data");
		auto w = getUInt32("width");
		auto h = getUInt32("height");
		auto d = getUInt32("depth");

		// Parse from data
		if (view.size() > 0 && w == 0 && h == 0) {
			stbi__context ctx;
			stbi__start_mem(&ctx, (stbi_uc*)view.data(), int(view.size()));
			{
				// GIF TEST
				auto gifParsed = stbi__gif_test(&ctx);
				if (gifParsed) {
					int c;
					stbi__gif gif;

					memset(&gif, 0, sizeof(stbi__gif));
					uint8_t* bin =
						stbi__gif_load_next(&ctx, &gif, &c, 4, nullptr);
					auto frames = list({});
					while (bin) {
						auto dSize = gif.w * gif.h * 4;
						auto frame = binary(dSize);
						memcpy(frame.data(), bin, dSize);
						auto fObj = obj({
							{"delay", gif.delay},
							{"data", frame},
							{"width", gif.w},
							{"height", gif.h},
						});
						frames.pushObject(fObj);
						bin =
							stbi__gif_load_next(&ctx, &gif, &c, 4, nullptr);
					}
					setList("frames", frames);
					setUInt32("width", ctx.img_x);
					setUInt32("height", ctx.img_y);
					setUInt32("depth", 0);
					setUInt32("format", bimg::TextureFormat::RGBA8);
					setUInt8("orientation", 0);
					setUInt16("numLayers", 1);
					setUInt8("numMips", 0);
					setBool("hasAlpha", true);
					setBool("cubeMap", false);
					setBool("ktx", false);
					setBool("ktxLE", false);
					setBool("srgb", false);
					return;
				}
			}
			{
				// STBI TEST
				int sw, sh, sc;
				stbi__result_info info;
				auto parsed = stbi__info_main(&ctx, &sw, &sh, &sc);
				if (parsed) {
					auto stbParsed =
						stbi__load_main(&ctx, &sw, &sh, &sc, sc, &info, 8);
					if (stbParsed != nullptr) {
						auto pBin =
							string_view((char*)stbParsed, sw * sh * sc);
						auto bin = binary(pBin.begin(), pBin.end());
						auto format = bimg::TextureFormat::R8;
						bool transparent = false;
						auto nC = info.num_channels;
						if (nC == 2) {
							format = bimg::TextureFormat::RG8;
						} else if (nC == 3) {
							if (info.bits_per_channel == 8)
								format = bimg::TextureFormat::RGB8;
						} else if (nC == 4 || nC == 0) {
							if (info.bits_per_channel == 8)
								format = bimg::TextureFormat::RGBA8;
							else if (info.bits_per_channel == 16)
								format = bimg::TextureFormat::RGBA16;
							transparent = true;
						}
						setBinary("data", bin);
						setUInt32("format", format);
						setUInt8("orientation", 0);
						setUInt32("width", sw);
						setUInt32("height", sh);
						setUInt32("depth", 0);
						setUInt16("numLayers", 1);
						setUInt8("numMips", 0);
						setBool("hasAlpha", transparent);
						setBool("cubeMap", false);
						setBool("ktx", false);
						setBool("ktxLE", false);
						setBool("srgb", false);
						return;
					}
				}
			}
			{
				// BIMG TEST
				auto err = (bx::Error*)(nullptr);
				auto bimgParsed = bimg::imageParse(
					con, view.data(), uint32_t(view.size()), err);
				if (bimgParsed) {
					binary bin;
					if (con.m_data) {
						bin = binary((size_t)con.m_size);
						memcpy(bin.data(), con.m_data, con.m_size);
					} else {
						auto start = view.begin();
						advance(start, (size_t)con.m_offset);
						bin = binary(
							(size_t)view.size() - (size_t)con.m_offset);
						memcpy(
							bin.data(), view.data() + (size_t)con.m_offset,
							view.size() - (size_t)con.m_offset);
					}
					setBinary("data", bin);
					setUInt32("format", con.m_format);
					setUInt8("orientation", uint8_t(con.m_orientation));
					setUInt32("width", con.m_width);
					setUInt32("height", con.m_height);
					setUInt32("depth", con.m_depth);
					setUInt16("numLayers", con.m_numLayers);
					setUInt8("numMips", con.m_numMips);
					setBool("hasAlpha", con.m_hasAlpha);
					setBool("cubeMap", con.m_cubeMap);
					setBool("ktx", con.m_ktx);
					setBool("srgb", con.m_srgb);
					return;
				}
			}
		}
		if (w > 0 && h > 0) {
			// Allocate memory (optional copy)
			auto numLayers = getUInt16("numLayers", 1);
			auto cubeMap = getBool("cubeMap");
			auto hasMips = getUInt16("numMips") > 0;
			auto format =
				(bimg::TextureFormat::Enum)getUInt32("format");
			auto allocCon = bimg::imageAlloc(
				&defaultAllocator, format, uint16_t(w), uint16_t(h),
				uint16_t(d), uint16_t(numLayers),
				cubeMap,
				hasMips, view.data());
			if (!allocCon) return;
			setPtr("con", allocCon);
			setStringView(
				"data",
				string_view((char*)allocCon->m_data, allocCon->m_size));
		}
	}

	bool image::isCompressed() {
		auto format =
			(bimg::TextureFormat::Enum)getUInt32("format");
		return bimg::isCompressed(format);
	}

	bool image::isColor() {
		auto format =
			(bimg::TextureFormat::Enum)getUInt32("format");
		return bimg::isColor(format);
	}

	bool image::isDepth() {
		auto format =
			(bimg::TextureFormat::Enum)getUInt32("format");
		return bimg::isDepth(format);
	}

	bool image::isValid() {
		auto format =
			(bimg::TextureFormat::Enum)getUInt32("format");
		return bimg::isValid(format);
	}

	bool image::isFloat() {
		auto format =
			(bimg::TextureFormat::Enum)getUInt32("format");
		return bimg::isFloat(format);
	}

	uint8_t image::getBitsPerPixel() {
		auto format =
			(bimg::TextureFormat::Enum)getUInt32("format");
		return bimg::getBitsPerPixel(format);
	}

	image image::checkerboard(
		uint32_t w, uint32_t h, uint32_t step, uint32_t _0,
		uint32_t _1) {
		auto img = image({
			{"width", w},
			{"height", h},
			{"format", bimg::TextureFormat::RGBA8},
		});
		auto con = img.getContainer();
		bimg::imageCheckerboard(
			con->m_data, con->m_width, con->m_height, step, _0, _1);
		return img;
	}
	image image::solid(uint32_t w, uint32_t h, uint32_t solid) {
		auto img = image({
			{"width", w},
			{"height", h},
			{"format", bimg::TextureFormat::RGBA8},
		});
		auto con = img.getContainer();
		bimg::imageSolid(
			con->m_data, con->m_width, con->m_height, solid);
		return img;
	}

	var image::getFormat(list) { return getVar("format"); }
	var image::getFormatName(list) {
		return bimg::getName(
			(bimg::TextureFormat::Enum)getUInt32("format"));
	}

	var image::convert(list args) {
		if (args.size() < 1)
			return genericError("convert requires a target format");
		auto targetFormat =
			(bimg::TextureFormat::Enum)args[0].getUInt32();
		auto mips = args[1].getBool(true);
		auto sourceFormat =
			(bimg::TextureFormat::Enum)getUInt32("format");
		auto con = getContainer();
		if (!con) return genericError("image has no valid data");
		if (bimg::imageConvert(targetFormat, sourceFormat)) {
			auto newCon = bimg::imageConvert(
				&defaultAllocator, targetFormat, *con, mips);
			if (newCon) {
				bimg::imageFree(con);
				setPtr("con", newCon);
				auto view =
					string_view((char*)newCon->m_data, newCon->m_size);
				setStringView("data", view);
				setUInt32("format", targetFormat);
			}
		}
		return *this;
	}
	var image::toLinearRGBA32F() {
		auto con = getContainer();
		if (!con) return genericError("image has no valid data");
		bimg::imageRgba32fToLinear(con);
		return *this;
	}
	var image::toGammaRGBA32F() {
		auto con = getContainer();
		if (!con) return genericError("image has no valid data");
		bimg::imageRgba32fToGamma(con);
		return *this;
	}
	var image::linearDownsampleRGBA32F2x2(list args) {
		auto img = image({
			{"width", args[0].getUInt32()},
			{"height", args[1].getUInt32()},
			{"depth", args[2].getUInt32()},
			{"format", bimg::TextureFormat::RGBA32F},
		});
		auto pitch = args[3].getUInt32();
		auto srcCon = getContainer();
		auto con = img.getContainer();
		bimg::imageRgba32fLinearDownsample2x2(
			con->m_data, con->m_width, con->m_height, con->m_depth,
			pitch, srcCon->m_data);
		return img;
	}
	var image::downsampleRGBA82x2(list args) {
		auto img = image({
			{"width", args[0].getUInt32()},
			{"height", args[1].getUInt32()},
			{"depth", args[2].getUInt32()},
			{"format", bimg::TextureFormat::RGBA32F},
		});
		auto srcPitch = args[3].getUInt32();
		auto dstPitch = args[4].getUInt32();
		auto srcCon = getContainer();
		auto con = img.getContainer();
		bimg::imageRgba8Downsample2x2(
			con->m_data, con->m_width, con->m_height, con->m_depth,
			srcPitch, dstPitch, srcCon->m_data);
		return img;
	}
	var image::downsampleRGBA32F2x2(list args) {
		auto img = image({
			{"width", args[0].getUInt32()},
			{"height", args[1].getUInt32()},
			{"depth", args[2].getUInt32()},
			{"format", bimg::TextureFormat::RGBA32F},
		});
		auto srcPitch = args[3].getUInt32();
		auto srcCon = getContainer();
		auto con = img.getContainer();
		bimg::imageRgba32fDownsample2x2(
			con->m_data, con->m_width, con->m_height, con->m_depth,
			srcPitch, srcCon->m_data);
		return img;
	}
	var image::downsampleRGBA32F2x2NormalMap(list args) {
		auto img = image({
			{"width", args[0].getUInt32()},
			{"height", args[1].getUInt32()},
			{"format", bimg::TextureFormat::RGBA32F},
		});
		auto srcPitch = args[2].getUInt32();
		auto dstPitch = args[3].getUInt32();
		auto srcCon = getContainer();
		auto con = img.getContainer();
		bimg::imageRgba32fDownsample2x2NormalMap(
			con->m_data, con->m_width, con->m_height, srcPitch,
			dstPitch, srcCon->m_data);
		return img;
	}
	var image::swizzleBGRA8(list args) {
		auto img = image({
			{"width", args[0].getUInt32()},
			{"height", args[1].getUInt32()},
			{"format", bimg::TextureFormat::RGBA32F},
		});
		auto srcPitch = args[2].getUInt32();
		auto dstPitch = args[3].getUInt32();
		auto srcCon = getContainer();
		auto con = img.getContainer();
		bimg::imageSwizzleBgra8(
			con->m_data, dstPitch, con->m_width, con->m_height,
			srcCon->m_data, srcPitch);
		return img;
	}
	var image::decodeToR8(list args) {
		auto img = image({
			{"width", args[0].getUInt32()},
			{"height", args[1].getUInt32()},
			{"depth", args[3].getUInt32()},
			{"format", bimg::TextureFormat::RGBA32F},
		});
		auto dstPitch = args[4].getUInt32();
		auto srcCon = getContainer();
		auto con = img.getContainer();
		bimg::imageDecodeToR8(
			&defaultAllocator, con->m_data, srcCon->m_data,
			con->m_width, con->m_height, con->m_depth, dstPitch,
			srcCon->m_format);
		return img;
	}
	var image::decodeToBGRA8(list args) {
		auto img = image({
			{"width", args[0].getUInt32()},
			{"height", args[1].getUInt32()},
			{"format", bimg::TextureFormat::RGBA32F},
		});
		auto dstPitch = args[2].getUInt32();
		auto srcCon = getContainer();
		auto con = img.getContainer();
		bimg::imageDecodeToBgra8(
			&defaultAllocator, con->m_data, srcCon->m_data,
			con->m_width, con->m_height, dstPitch, srcCon->m_format);
		return img;
	}
	var image::decodeToRGBA8(list args) {
		auto img = image({
			{"width", args[0].getUInt32()},
			{"height", args[1].getUInt32()},
			{"format", bimg::TextureFormat::RGBA32F},
		});
		auto dstPitch = args[2].getUInt32();
		auto srcCon = getContainer();
		auto con = img.getContainer();
		bimg::imageDecodeToRgba8(
			&defaultAllocator, con->m_data, srcCon->m_data,
			con->m_width, con->m_height, dstPitch, srcCon->m_format);
		return img;
	}
	var image::decodeToRGBA32F(list args) {
		auto img = image({
			{"width", args[0].getUInt32()},
			{"height", args[1].getUInt32()},
			{"depth", args[2].getUInt32()},
			{"format", bimg::TextureFormat::RGBA32F},
		});
		auto dstPitch = args[3].getUInt32();
		auto srcCon = getContainer();
		auto con = img.getContainer();
		bimg::imageDecodeToRgba32f(
			&defaultAllocator, con->m_data, srcCon->m_data,
			con->m_width, con->m_height, con->m_depth, dstPitch,
			srcCon->m_format);
		return img;
	}
	var image::getRawData(list args) {
		if (args.size() < 2)
			return genericError("getRawData requires side and level");
		auto con = getContainer();
		if (!con) return genericError("image has no valid data");
		auto side = args[0].getUInt16();
		auto lod = args[1].getUInt8();
		auto mip = bimg::ImageMip();
		auto got = bimg::imageGetRawData(
			*con, side, lod, con->m_data, con->m_size, mip);
		if (got) {
			return image({
				{"blockSize", mip.m_blockSize},
				{"bpp", mip.m_bpp},
				{"data", mip.m_data},
				{"depth", mip.m_depth},
				{"format", mip.m_format},
				{"hasAlhpa", mip.m_hasAlpha},
				{"height", mip.m_height},
				{"size", mip.m_size},
				{"width", mip.m_width},
			});
		}
		return var();
	}

	var image::writeTGA(list args) {
		auto block = bx::MemoryBlock(&defaultAllocator);
		auto writer = bx::MemoryWriter(&block);
		auto con = getContainer();
		auto grayscale = args[0].getBool();
		auto yflip = args[1].getBool();
		auto error = bx::Error();
		auto size = bimg::imageWriteTga(
			&writer, con->m_width, con->m_height, 0, con->m_data,
			grayscale, yflip, &error);
		if (!error.isOk()) {
			auto err = error.getMessage();
			return genericError(
				string(err.getCPtr(), err.getLength()));
		}
		auto bin = binary(size);
		memcpy(bin.data(), block.more(), size);
		return bin;
	}
	var image::writePNG(list args) {
		auto block = bx::MemoryBlock(&defaultAllocator);
		auto writer = bx::MemoryWriter(&block);
		auto con = getContainer();
		auto yflip = args[0].getBool();
		auto error = bx::Error();
		auto size = bimg::imageWritePng(
			&writer, con->m_width, con->m_height, 0, con->m_data,
			con->m_format, yflip, &error);
		if (!error.isOk()) {
			auto err = error.getMessage();
			return genericError(
				string(err.getCPtr(), err.getLength()));
		}
		auto bin = binary(size);
		memcpy(bin.data(), block.more(), size);
		return bin;
	}
	var image::writeEXR(list args) {
		auto block = bx::MemoryBlock(&defaultAllocator);
		auto writer = bx::MemoryWriter(&block);
		auto con = getContainer();
		auto yflip = args[0].getBool();
		auto error = bx::Error();
		auto size = bimg::imageWriteExr(
			&writer, con->m_width, con->m_height, 0, con->m_data,
			con->m_format, yflip, &error);
		if (!error.isOk()) {
			auto err = error.getMessage();
			return genericError(
				string(err.getCPtr(), err.getLength()));
		}
		auto bin = binary(size);
		memcpy(bin.data(), block.more(), size);
		return bin;
	}
	var image::writeHDR(list args) {
		auto block = bx::MemoryBlock(&defaultAllocator);
		auto writer = bx::MemoryWriter(&block);
		auto con = getContainer();
		auto yflip = args[0].getBool();
		auto error = bx::Error();
		auto size = bimg::imageWriteHdr(
			&writer, con->m_width, con->m_height, 0, con->m_data,
			con->m_format, yflip, &error);
		if (!error.isOk()) {
			auto err = error.getMessage();
			return genericError(
				string(err.getCPtr(), err.getLength()));
		}
		auto bin = binary(size);
		memcpy(bin.data(), block.more(), size);
		return bin;
	}
	var image::writeDDS(list) {
		auto block = bx::MemoryBlock(&defaultAllocator);
		auto writer = bx::MemoryWriter(&block);
		auto con = getContainer();
		auto error = bx::Error();
		auto size = bimg::imageWriteDds(
			&writer, *con, con->m_data, con->m_size, &error);
		if (!error.isOk()) {
			auto err = error.getMessage();
			return genericError(
				string(err.getCPtr(), err.getLength()));
		}
		auto bin = binary(size);
		memcpy(bin.data(), block.more(), size);
		return bin;
	}
	var image::writeKTX(list) {
		auto block = bx::MemoryBlock(&defaultAllocator);
		auto writer = bx::MemoryWriter(&block);
		auto con = getContainer();
		auto error = bx::Error();
		auto size = bimg::imageWriteKtx(
			&writer, *con, con->m_data, con->m_size, &error);
		if (!error.isOk()) {
			auto err = error.getMessage();
			return genericError(
				string(err.getCPtr(), err.getLength()));
		}
		auto bin = binary(size);
		memcpy(bin.data(), block.more(), size);
		return bin;
	}

	var image::destroy() {
		auto con = (bimg::ImageContainer*)getPtr("con");
		if (con) bimg::imageFree(con);
		empty();
		return var();
	}
}  // namespace gold
