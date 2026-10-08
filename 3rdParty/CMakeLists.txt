cmake_minimum_required(VERSION 3.16...4.2)

project(gold)

cmake_policy(SET CMP0079 NEW)

if(NOT MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-gnu-zero-variadic-macro-arguments")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-gnu-zero-variadic-macro-arguments")
endif(NOT MSVC)

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
	include(cmake/uSockets.cmake)
	include(cmake/uWebSockets.cmake)
endif()

# ------------------------------------------------------------ game module deps
if(GOLD_BUILD_GAME)
	include(cmake/bullet.cmake)
endif()

# FreeType is NOT built here: the UI module opens the system FreeType at
# runtime through the plugin loader (src/ui/ftShim*), falling back to the
# built-in 5x7 font when it is unavailable.
