cmake_minimum_required(VERSION 3.16)

project(gold)

cmake_policy(SET CMP0079 NEW)

if(!MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-gnu-zero-variadic-macro-arguments")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-gnu-zero-variadic-macro-arguments")
endif(!MSVC)

# Locate the bgfx family of libraries up front so the core (goldShared) can
# build bx/bimg without pulling in the whole game stack.
set(BX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bx" CACHE STRING "Location of bx.")
set(BIMG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bimg" CACHE STRING "Location of bimg.")
set(BGFX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bgfx" CACHE STRING "Location of bgfx.")

# ---------------------------------------------------------------- core deps
# goldShared depends on bx/bimg (from the system bgfx package when
# GOLD_USE_SYSTEM_BGFX, else the bundled submodules) and ships its own
# JSON/BSON/CBOR/MsgPack/UBJSON codecs (src/goldjson.cpp).
if(NOT GOLD_USE_SYSTEM_BGFX)
	if(NOT TARGET bx)
		include(cmake/bx.cmake)
	endif()
	if(NOT TARGET bimg)
		include(cmake/bimg.cmake)
	endif()
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
	add_subdirectory(freetype2)
	include(cmake/bullet.cmake)
	if(NOT GOLD_USE_SYSTEM_BGFX)
		include(cmake/brtshaderc.cmake)
		include(cmake/bgfxConfig.cmake)
	endif()
endif()