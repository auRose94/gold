
cmake_minimum_required(VERSION 3.16)

# Some bundled 3rdParty submodules still declare pre-3.5 minimums
# (removed from CMake). Allow CMake to configure them with modern policies.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project(gold VERSION 0.1.0)


set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
set(BUILD_TESTING OFF CACHE INTERNAL "")
set(BGFX_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(BGFX_CONFIG_DEBUG ON CACHE INTERNAL "")
set(ENTRY_CONFIG_USE_SDL ON CACHE INTERNAL "")
set(BGFX_OPENGL_VERSION "46" CACHE INTERNAL "")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
find_package(OpenSSL REQUIRED)
set(USE_MSVC_RUNTIME_LIBRARY_DLL ON CACHE INTERNAL "")
set(BUILD_BULLET2_DEMOS OFF CACHE INTERNAL "")
set(BUILD_UNIT_TESTS OFF CACHE INTERNAL "")
set(OpenGL_GL_PREFERENCE "GLVND" CACHE INTERNAL "")
set(MONGOC_ENABLE_STATIC_BUILD ON CACHE INTERNAL "")
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
option(GOLD_BUILD_TESTS "Build the gold test suite" ON)
option(GOLD_BUILD_EXAMPLES "Build the gold example projects" OFF)

set(GOLD_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(3rdParty)


unset(_MSC_VER CACHE)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)

include(cmake/shared.cmake)
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
	if(NOT GOLD_BUILD_GAME)
		message(FATAL_ERROR "GOLD_BUILD_EXAMPLES requires GOLD_BUILD_GAME")
	endif()
	add_subdirectory(examples/conwaysGameOfLife)
	if(GOLD_BUILD_WEB)
		add_subdirectory(examples/myWebProject)
	endif()
endif()