
cmake_minimum_required(VERSION 3.16...4.2)

project(gold CXX)

# The UI module turns parsed HTML/CSS into pixels: style cascade, layout
# (block/inline + flex + grid), text shaping, a CPU rasterizer, and an
# event/hit-test surface for game interaction. gold::game adds a thin
# adapter (uiSurface) on top of this; the module itself has no GPU
# dependency so it can be tested headless.
#
# The `software_renderer` (CPU rasterizer) is the primary, always-on backend.
# `ultralight_renderer` (WebKit) is an optional secondary backend; it pulls in
# the Ultralight SDK, so it is off by default. Enable with
# -DGOLD_UI_ULTRALIGHT=ON.
option(GOLD_UI_ULTRALIGHT "Build the Ultralight (WebKit) UI renderer backend" OFF)

set(goldUI_sources
	src/ui/font.cpp
	src/ui/layout.cpp
	src/ui/raster.cpp
	src/ui/renderer.cpp
	src/ui/style.cpp
	src/ui/tree.cpp
)
if(GOLD_UI_ULTRALIGHT)
	list(APPEND goldUI_sources src/ui/ultralight_renderer.cpp)
endif()

add_library(
	goldUI
	SHARED
		${goldUI_sources}
)
add_library(
	gold::ui ALIAS goldUI
)

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
		"include"
		"include/ui"
	PRIVATE
		3rdParty/freetype2/include
)

if(GOLD_UI_ULTRALIGHT)
	get_filename_component(ultralight_root "${CMAKE_SOURCE_DIR}/3rdParty/ultralight-free-sdk" ABSOLUTE)
	target_include_directories(
		goldUI
		PRIVATE
			${ultralight_root}/include
	)
	target_link_libraries(
		goldUI
		PRIVATE
			${ultralight_root}/bin/libUltralight.so
			${ultralight_root}/bin/libUltralightCore.so
	)
	set_target_properties(
		goldUI
		PROPERTIES
			BUILD_RPATH "$ORIGIN;${ultralight_root}/bin"
			INSTALL_RPATH "${ultralight_root}/bin"
	)
endif()

# gold::web carries the HTML/CSS parsers; freetype carries the glyph
# rasterizer. Neither is exposed through goldUI's public headers.
target_link_libraries(
	goldUI
	PUBLIC
		gold::shared
	PRIVATE
		gold::web
		freetype
)

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
