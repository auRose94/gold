
cmake_minimum_required(VERSION 3.16)

# Some bundled 3rdParty submodules still declare pre-3.5 minimums
# (removed from CMake). Allow CMake to configure them with modern policies.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project(gold VERSION 0.1.0)

# Keep legacy bundled dependency behavior explicit when its nested
# CMakeLists.txt resets the policy scope.
set(CMAKE_POLICY_DEFAULT_CMP0160 OLD)
set(CMAKE_POLICY_DEFAULT_CMP0115 OLD)
if(POLICY CMP0218)
	cmake_policy(SET CMP0218 NEW)
	set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL
		"Suppress deprecation noise from bundled legacy projects" FORCE)
endif()


set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
set(BUILD_TESTING OFF CACHE INTERNAL "")
set(BGFX_BUILD_EXAMPLES OFF CACHE INTERNAL "")
# gold builds its own shaderc (cmake/shaderc.cmake) with a link fix for the
# circular shader-toolchain archives, so disable the bgfx.cmake submodule's
# built-in shaderc build.
set(BGFX_BUILD_TOOLS_SHADER OFF CACHE INTERNAL "")
set(BGFX_CONFIG_DEBUG ON CACHE INTERNAL "")
set(ENTRY_CONFIG_USE_SDL ON CACHE INTERNAL "")
set(BGFX_OPENGL_VERSION "46" CACHE INTERNAL "")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
find_package(OpenSSL REQUIRED)
set(USE_MSVC_RUNTIME_LIBRARY_DLL ON CACHE INTERNAL "")
set(BUILD_BULLET2_DEMOS OFF CACHE INTERNAL "")
set(BUILD_UNIT_TESTS OFF CACHE INTERNAL "")
set(OpenGL_GL_PREFERENCE "GLVND" CACHE INTERNAL "")

set(ENABLE_TESTS OFF CACHE INTERNAL "")
set(ENABLE_EXAMPLES OFF CACHE INTERNAL "")
set(BUILD_CPU_DEMOS OFF CACHE INTERNAL "")
set(BUILD_OPENGL3_DEMOS OFF CACHE INTERNAL "")
set(ENABLE_UNINSTALL OFF CACHE INTERNAL "")
set(INSTALL_LIBS OFF CACHE INTERNAL "")
set(INSTALL_EXTRA_LIBS OFF CACHE INTERNAL "")
set(INSTALL_CMAKE_FILES OFF CACHE INTERNAL "")
set(BGFX_INSTALL OFF CACHE INTERNAL "")
set(LIBUV_BUILD_TESTS OFF CACHE INTERNAL "")
set(LIBUV_BUILD_BENCH OFF CACHE INTERNAL "")

# --------------------------------------------------------------------------
# Build options. Turn these off to build only the shared core (fastest, and
# what the test suite needs).
# --------------------------------------------------------------------------
option(GOLD_BUILD_GAME "Build the gold game engine module" ON)
option(GOLD_BUILD_WEB "Build the gold web server module" ON)
option(GOLD_BUILD_LANG "Build the gold scripting-language module" ON)
option(GOLD_BUILD_TESTS "Build the gold test suite" ON)
option(GOLD_BUILD_EXAMPLES "Build the gold example projects" OFF)

# bgfx/bx/bimg: prefer an installed system bgfx (shared lib + headers +
# the bgfx-shaderc tool) over building the bundled submodules. Using the
# system install cuts a lot of compile time (amalgamated bgfx plus the
# shader-compiler stack glslang/spirv-tools/glsl-optimizer/fcpp). When no
# system bgfx is found the bundled sources are built instead.
find_package(bgfx QUIET)
option(GOLD_USE_SYSTEM_BGFX
	"Use the system bgfx install (libbgfx.so + bgfx-shaderc) instead of the bundled submodules"
	${bgfx_FOUND})
if(GOLD_USE_SYSTEM_BGFX AND NOT bgfx_FOUND)
	message(FATAL_ERROR "GOLD_USE_SYSTEM_BGFX=ON but find_package(bgfx) found no system install")
endif()
if(GOLD_USE_SYSTEM_BGFX)
	set(GOLD_BGFX_TARGET bgfx::bgfx)
	set(GOLD_BX_TARGET bgfx::bx)
	set(GOLD_BIMG_TARGET bgfx::bimg)
	set(GOLD_SHADER_COMPILER "/usr/bin/bgfx-shaderc")
	# bgfx's shader header includes (bgfx_shader.sh, bgfx_compute.sh).
	set(GOLD_BGFX_SHADER_INCLUDE "/usr/include/bgfx")
else()
	set(GOLD_BGFX_TARGET bgfx)
	set(GOLD_BX_TARGET bx)
	set(GOLD_BIMG_TARGET bimg)
	set(GOLD_SHADER_COMPILER "$<TARGET_FILE:shaderc>")
	set(GOLD_BGFX_SHADER_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/bgfx.cmake/bgfx/src")
endif()

set(GOLD_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(3rdParty)


unset(_MSC_VER CACHE)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 26)

# Export a full compile_commands.json (all C++ targets, not just the C
# files that libuv's nested scope exports) so clangd/LSP resolves the
# correct standard and include paths.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/shared.cmake)
if(GOLD_BUILD_LANG)
	include(cmake/lang.cmake)
endif()
if(GOLD_BUILD_GAME)
	include(cmake/shaders.cmake)
	include(cmake/game.cmake)
endif()
if(GOLD_BUILD_WEB)
	include(cmake/web.cmake)
endif()
if(GOLD_BUILD_TESTS)
	include(cmake/tests.cmake)
endif()
if(GOLD_BUILD_EXAMPLES)
	add_subdirectory(examples/langExample)
	add_subdirectory(examples/blahajExample)
	if(NOT GOLD_BUILD_GAME)
		message(FATAL_ERROR "GOLD_BUILD_EXAMPLES requires GOLD_BUILD_GAME")
	endif()
	add_subdirectory(examples/conwaysGameOfLife)
	if(GOLD_BUILD_WEB)
		add_subdirectory(examples/myWebProject)
	endif()
endif()
