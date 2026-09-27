cmake_minimum_required(VERSION 3.16...4.2)

project(gold)

cmake_policy(SET CMP0079 NEW)

if(!MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-gnu-zero-variadic-macro-arguments")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-gnu-zero-variadic-macro-arguments")
endif(!MSVC)

# ---------------------------------------------------------------- core deps
# goldShared depends on bx/bimg (from the system bgfx package when
# GOLD_USE_SYSTEM_BGFX, else the bundled bgfx.cmake port) and ships its own
# JSON/BSON/CBOR/MsgPack/UBJSON codecs (src/goldjson.cpp).
if(NOT GOLD_USE_SYSTEM_BGFX)
	# bgfx.cmake builds bx, bimg and bgfx from a single add_subdirectory.
	add_subdirectory(bgfx.cmake)
	# Build the shader toolchain libs (glslang/tint/spirv-opt/spirv-cross/
	# webgpu) and gold's own shaderc tool. The submodule's shaderc build is
	# disabled (BGFX_BUILD_TOOLS_SHADER=OFF); gold's cmake/shaderc.cmake
	# carries the link fix for the circular toolchain archives.
	include(${CMAKE_CURRENT_SOURCE_DIR}/bgfx.cmake/cmake/bgfx/3rdparty/spirv-opt.cmake)
	include(${CMAKE_CURRENT_SOURCE_DIR}/bgfx.cmake/cmake/bgfx/3rdparty/spirv-cross.cmake)
	include(${CMAKE_CURRENT_SOURCE_DIR}/bgfx.cmake/cmake/bgfx/3rdparty/glslang.cmake)
	include(${CMAKE_CURRENT_SOURCE_DIR}/bgfx.cmake/cmake/bgfx/3rdparty/webgpu.cmake)
	include(${CMAKE_CURRENT_SOURCE_DIR}/bgfx.cmake/cmake/bgfx/3rdparty/tint.cmake)
	include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/shaderc.cmake)
endif()

# ------------------------------------------------------------ web module deps
if(GOLD_BUILD_WEB)
	add_subdirectory(zlib)
	add_subdirectory(libuv)
	include(cmake/uSockets.cmake)
	include(cmake/uWebSockets.cmake)
endif()

# ------------------------------------------------------------ game module deps
if(GOLD_BUILD_GAME)
	include(cmake/bullet.cmake)
endif()

# ------------------------------------------------- ui module deps (freetype)
# The UI module rasterizes text through the bundled FreeType, so build it
# whenever the module is enabled (gold::game also uses it via SDL_ttf).
if(GOLD_BUILD_UI AND GOLD_BUILD_WEB)
	add_subdirectory(freetype2)
endif()
