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
# goldShared depends on: json, bx, bimg. (Crypto via system OpenSSL.)
add_subdirectory(json)
if(NOT TARGET bx)
	include(cmake/bx.cmake)
endif()
if(NOT TARGET bimg)
	include(cmake/bimg.cmake)
endif()

# ------------------------------------------------------------ web module deps
if(GOLD_BUILD_WEB)
	add_subdirectory(zlib)
	add_subdirectory(libuv)
	# CMake 4.x rejects the mongo driver's TRY_COMPILE hack that smuggles
	# a fake linker through "-Werror -DCMAKE_CXX_LINK_EXECUTABLE=...".
	# Patch that one flag pair at configure time (then restore) so the
	# vendored driver configures cleanly without dirtying the submodule.
	set(_MONGOC_CMAKE_SRC
		"${CMAKE_CURRENT_SOURCE_DIR}/mongo-c-driver/src/libmongoc/CMakeLists.txt")
	if(EXISTS "${_MONGOC_CMAKE_SRC}")
		file(READ "${_MONGOC_CMAKE_SRC}" _MONGOC_CMAKE)
		string(REPLACE
			"-Werror -DCMAKE_CXX_LINK_EXECUTABLE='echo not linking now...'"
			"COMPILE_DEFINITIONS -Werror CMAKE_FLAGS -DCMAKE_CXX_LINK_EXECUTABLE=echo"
			_MONGOC_PATCHED "${_MONGOC_CMAKE}")
		if(NOT "${_MONGOC_PATCHED}" STREQUAL "${_MONGOC_CMAKE}")
			file(WRITE "${_MONGOC_CMAKE_SRC}" "${_MONGOC_PATCHED}")
			set(_MONGOC_RESTORE "${_MONGOC_CMAKE}")
		endif()
	endif()
	add_subdirectory(mongo-c-driver)
	if(DEFINED _MONGOC_RESTORE)
		file(WRITE "${_MONGOC_CMAKE_SRC}" "${_MONGOC_RESTORE}")
		unset(_MONGOC_RESTORE)
	endif()
	include(cmake/uSockets.cmake)
	include(cmake/uWebSockets.cmake)
endif()

# ------------------------------------------------------------ game module deps
if(GOLD_BUILD_GAME)
	add_subdirectory(freetype2)
	# SDL's Wayland driver uses dynamic loading, but its generated protocol
	# stubs reference wl_proxy_marshal_flags directly, which breaks static
	# linking. gold's own backend uses the X11 path, so build SDL with X11.
	set(VIDEO_WAYLAND OFF CACHE BOOL "Use Wayland video driver" FORCE)
	set(VIDEO_X11 ON CACHE BOOL "Use X11 video driver" FORCE)
	# goldGame is a shared library, so SDL2-static must be built with PIC.
	set(SDL_STATIC_PIC ON CACHE BOOL "Build static SDL with PIC" FORCE)
	add_subdirectory(SDL)
	add_subdirectory(SDL_image)
	add_subdirectory(SDL_ttf)
	include(cmake/bullet.cmake)
	include(cmake/brtshaderc.cmake)
	include(cmake/bgfxConfig.cmake)
endif()