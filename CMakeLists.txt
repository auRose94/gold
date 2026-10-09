
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

# bgfx/bx/bimg come from the system bgfx package (headers + renderer
# lib + the bgfx-shaderc tool). gold no longer vendors the bgfx sources
# and carries none of the renderer in the core binaries: the bgfx
# backend ships as the loadable libgoldBgfx plugin (libgoldGame keeps
# only the bx/bimg header and container-decode use). The BUILD still
# needs the package while the engine's embedded shaders are bgfx .sc
# blobs; the SDL_GPU parity path is the follow-up that drops this.
find_package(bgfx QUIET)
if(GOLD_BUILD_GAME AND NOT bgfx_FOUND)
	message(FATAL_ERROR
		"the gold game module needs the system bgfx package (the "
		"bgfx-shaderc tool for its embedded shaders + libbgfx for the "
		"goldBgfx render plugin, e.g. the bgfx-cmake package on Arch); "
		"build with GOLD_BUILD_GAME=OFF or use the SDL_GPU render "
		"parity path")
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
# Cross-module PRIVATE dependencies resolve in the build tree with
# $ORIGIN-relative rpaths (an example links only gold::game, so
# libgoldGame -> libgoldUI must not depend on the example's own
# runpath); install rewrites these to the same-dir $ORIGIN form.
set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)

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

# -----------------------------------------------------------------------------
# Install + export. gold installs as: headers, the core static library, the
# shared module libraries, and the loadable plugins in the SAME lib dir --
# the plugin loader's module-anchored search path points there, so plugins
# ship next to the modules they extend. find_package(gold) imports the
# module targets under their in-tree names (gold::shared, gold::web, ...).
# -----------------------------------------------------------------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(gold_INSTALL_LIBS goldShared)
if(TARGET goldWeb)
	list(APPEND gold_INSTALL_LIBS goldWeb)
	install(DIRECTORY include/web/
		DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/web)
endif()
if(TARGET goldUI)
	list(APPEND gold_INSTALL_LIBS goldUI)
	install(DIRECTORY include/ui/
		DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ui)
endif()
if(TARGET goldLang)
	list(APPEND gold_INSTALL_LIBS goldLang)
	install(DIRECTORY include/lang/
		DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lang)
endif()
if(TARGET goldGame)
	list(APPEND gold_INSTALL_LIBS goldGame)
	install(DIRECTORY include/game/
		DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/game)
endif()

# The module targets' interfaces carry genex-wrapped include dirs (the
# export needs install-tree paths) and depend only on each other and
# gold::shared plus the system packages the config file finds below.
# The module libraries carry no RPATH of their own: build-tree users
# (tests, examples) link through executables whose own rpath covers the
# modules' deps, and installed trees rely on the consumer's search
# path. This also keeps cmake's link-path rpath off dependent shared
# libs, whose trailing-empty-component trips the install-time
# RPATH_CHANGE check ("no valid ELF RPATH entry").
foreach(goldLib goldShared goldWeb goldUI goldLang goldGame)
	if(TARGET ${goldLib})
		set_target_properties(${goldLib} PROPERTIES INSTALL_RPATH "$ORIGIN")
	endif()
endforeach()
install(
	TARGETS ${gold_INSTALL_LIBS}
	EXPORT goldTargets
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
# Optional plugins land beside the modules (loader search path).
foreach(goldPlugin goldLws goldSDL3 goldSDL2 goldBullet goldBgfx goldUIUltralight)
	if(TARGET ${goldPlugin})
		set_target_properties(${goldPlugin} PROPERTIES INSTALL_RPATH "$ORIGIN")
		install(TARGETS ${goldPlugin}
			LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
	endif()
endforeach()

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Export under the in-tree alias names (gold::web, gold::ui, ...).
set_target_properties(goldShared PROPERTIES EXPORT_NAME shared)
if(TARGET goldWeb)
	set_target_properties(goldWeb PROPERTIES EXPORT_NAME web)
endif()
if(TARGET goldUI)
	set_target_properties(goldUI PROPERTIES EXPORT_NAME ui)
endif()
if(TARGET goldLang)
	set_target_properties(goldLang PROPERTIES EXPORT_NAME lang)
endif()
if(TARGET goldGame)
	set_target_properties(goldGame PROPERTIES EXPORT_NAME game)
endif()

install(
	EXPORT goldTargets
	FILE goldTargets.cmake
	NAMESPACE gold::
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gold
)
configure_package_config_file(
	cmake/goldConfig.cmake.in
	"${CMAKE_CURRENT_BINARY_DIR}/cmake/goldConfig.cmake"
	INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gold
)
write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/cmake/goldConfigVersion.cmake"
	# (the module cmakes call project() too and reset PROJECT_VERSION)
	VERSION 0.1.0
	COMPATIBILITY SameMajorVersion
)
install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/cmake/goldConfig.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/cmake/goldConfigVersion.cmake"
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gold
)
