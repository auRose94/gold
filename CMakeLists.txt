
cmake_minimum_required(VERSION 3.16)

# Some bundled 3rdParty submodules still declare pre-3.5 minimums
# (removed from CMake). Allow CMake to configure them with modern policies.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project(gold VERSION 0.1.0)

# An unset build type means no optimization flags at all; pick one so a
# plain `cmake -B build` still produces a working (release) build.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

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
# ABI mirror: the system libbgfx is built with debug config annotations.
set(BGFX_CONFIG_DEBUG ON CACHE INTERNAL "")
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

# --------------------------------------------------------------------------
# Build options. Turn these off to build only the shared core (fastest, and
# what the test suite needs).
# --------------------------------------------------------------------------
option(GOLD_BUILD_GAME "Build the gold game engine module" ON)
option(GOLD_BUILD_WEB "Build the gold web server module" ON)
option(GOLD_BUILD_UI "Build the gold HTML/CSS UI renderer module" ON)
option(GOLD_BUILD_LANG "Build the gold scripting-language module" ON)
option(GOLD_BUILD_TESTS "Build the gold test suite" ON)
option(GOLD_BUILD_EXAMPLES "Build the gold example projects" OFF)

# bgfx/bx/bimg come from the system bgfx package (shared lib + headers +
# the bgfx-shaderc tool). gold no longer vendors the bgfx sources: the
# render backend is a loadable plugin compiled against these system
# headers, so the core binaries carry none of them. Without the package,
# the game module cannot be built (the SDL_GPU render plugin is the
# documented parity alternative on platforms without a bgfx package).
find_package(bgfx QUIET)
if(GOLD_BUILD_GAME AND NOT bgfx_FOUND)
	message(FATAL_ERROR
		"gold::game needs the system bgfx package (libbgfx + headers + the "
		"bgfx-shaderc tool, e.g. the bgfx-cmake package on Arch); build "
		"with GOLD_BUILD_GAME=OFF, or use the SDL_GPU render parity path")
endif()
set(GOLD_BGFX_TARGET bgfx::bgfx)
set(GOLD_BX_TARGET bgfx::bx)
set(GOLD_BIMG_TARGET bgfx::bimg)
# The shader compiler is the system tool; shaders compile at build time.
set(GOLD_SHADER_COMPILER "/usr/bin/bgfx-shaderc")
# bgfx's shader header includes (bgfx_shader.sh, bgfx_compute.sh).
set(GOLD_BGFX_SHADER_INCLUDE "/usr/include/bgfx")

set(GOLD_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(3rdParty)


unset(_MSC_VER CACHE)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 26)

# Export a full compile_commands.json (all C++ targets) so clangd/LSP
# resolves the correct standard and include paths.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/shared.cmake)
# Order matters for the module graph: web (HTML/CSS parsers) -> ui (renderer,
# depends on web) -> game (engine, optional uiSurface adapter on top of ui).
if(GOLD_BUILD_LANG)
	include(cmake/lang.cmake)
endif()
if(GOLD_BUILD_WEB)
	include(cmake/web.cmake)
endif()
if(GOLD_BUILD_UI AND GOLD_BUILD_WEB)
	include(cmake/ui.cmake)
endif()
if(GOLD_BUILD_GAME)
	include(cmake/shaders.cmake)
	include(cmake/game.cmake)
endif()
if(GOLD_BUILD_TESTS)
	include(cmake/tests.cmake)
endif()
if(GOLD_BUILD_EXAMPLES)
	# Guard first: configuring an example whose dependency module is off
	# would otherwise fail deep inside that example's own CMakeLists.
	if(NOT GOLD_BUILD_GAME)
		message(FATAL_ERROR "GOLD_BUILD_EXAMPLES requires GOLD_BUILD_GAME")
	endif()
	add_subdirectory(examples/langExample)
	add_subdirectory(examples/blahajExample)
	add_subdirectory(examples/conwaysGameOfLife)
	if(GOLD_BUILD_WEB)
		add_subdirectory(examples/myWebProject)
	endif()
endif()
