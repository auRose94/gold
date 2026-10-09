
cmake_minimum_required(VERSION 3.16...4.2)

project(gold CXX)

# FreeType comes from the system install (dev headers only — the library
# is opened at runtime through the plugin loader, so a machine without it
# runs the built-in font). Without the headers, the inert shim keeps the
# module building.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
	pkg_check_modules(GOLD_FREETYPE QUIET freetype2)
endif()
set(goldUI_sources
	src/ui/font.cpp
	src/ui/layout.cpp
	src/ui/raster.cpp
	src/ui/renderer.cpp
	src/ui/style.cpp
	src/ui/tree.cpp
)
if(GOLD_FREETYPE_FOUND)
	list(APPEND goldUI_sources src/ui/ftShimFreetype.cpp)
else()
	list(APPEND goldUI_sources src/ui/ftShimNone.cpp)
	message(STATUS "gold: no system freetype2 headers; the UI module will use the built-in 5x7 font only")
endif()

# The UI module turns parsed HTML/CSS into pixels: style cascade, layout
# (block/inline + flex + grid), text shaping, a CPU rasterizer, and an
# event/hit-test surface for game interaction. gold::game adds a thin
# adapter (uiSurface) on top of this; the module itself has no GPU
# dependency so it can be tested headless. Renderer backends register into
# the renderer registry (see src/ui/renderer.cpp); optional WebKit-class
# backends ship as their own loadable plugins.

add_library(
	goldUI
	SHARED
		${goldUI_sources}
)
add_library(
	gold::ui ALIAS goldUI
)

# The Ultralight (WebKit) renderer is a loadable backend plugin, not part
# of goldUI: enabling it builds libgoldUltralight.so next to the modules.
# Requires the (gitignored) local SDK at 3rdParty/ultralight-free-sdk —
# the one pre-existing local-SDK exception to the system-package policy.
option(GOLD_UI_ULTRALIGHT
	"Build the Ultralight (WebKit) UI renderer plugin libgoldUltralight" OFF)
if(GOLD_UI_ULTRALIGHT)
	get_filename_component(ultralight_root
		"${CMAKE_SOURCE_DIR}/3rdParty/ultralight-free-sdk" ABSOLUTE)
	if(NOT EXISTS "${ultralight_root}/bin/libUltralight.so")
		message(FATAL_ERROR
			"GOLD_UI_ULTRALIGHT=ON but no Ultralight SDK found at "
			"${ultralight_root}")
	endif()
	add_library(
		goldUIUltralight
		SHARED
			src/ui/ultralight_renderer.cpp
	)
	add_library(
		gold::uiUltralight ALIAS goldUIUltralight
	)
	set_target_properties(
		goldUIUltralight
		PROPERTIES
			OUTPUT_NAME libgoldUltralight
			PREFIX ""
			LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
			BUILD_RPATH "$ORIGIN;${ultralight_root}/bin"
			INSTALL_RPATH "${ultralight_root}/bin"
	)
	target_include_directories(
		goldUIUltralight
		PRIVATE
			"include"
			"include/ui"
			${ultralight_root}/include
	)
	target_link_libraries(
		goldUIUltralight
		PRIVATE
			gold::ui
			${ultralight_root}/bin/libUltralight.so
			${ultralight_root}/bin/libUltralightCore.so
	)
endif()

set_target_properties(
	goldUI
	PROPERTIES
		VERSION 0.1.0
		SOVERSION 0
)

if(MSVC)
  target_compile_options(goldUI PRIVATE /W4 -Wno-unused-function -Wno-unused-variable)
else()
  target_compile_options(goldUI PRIVATE -Wall -Wextra -pedantic)
endif()

target_include_directories(
	goldUI
	PUBLIC
		# Genex-wrapped for the export (see shared.cmake).
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include/ui>
		$<INSTALL_INTERFACE:include>
		$<INSTALL_INTERFACE:include/ui>
)

# gold::web carries the HTML/CSS parsers; the freetype shim handles
# glyphs. Neither is exposed through goldUI's public headers.
target_link_libraries(
	goldUI
	PUBLIC
		gold::shared
	PRIVATE
		gold::web
)

if(GOLD_FREETYPE_FOUND)
	# Compile-time access to the C ABI headers; the library itself is
	# opened at runtime (see the freetype shim).
	target_include_directories(goldUI PRIVATE ${GOLD_FREETYPE_INCLUDE_DIRS})
endif()

target_compile_features(
	goldUI
	PUBLIC
		cxx_variadic_templates
		cxx_nullptr
		cxx_generic_lambdas
		cxx_lambdas
		cxx_auto_type
		cxx_variable_templates
		cxx_variadic_macros
		cxx_template_template_parameters
		cxx_std_26
)
