
cmake_minimum_required(VERSION 3.16...4.2)

project(gold CXX)

# The UI module turns parsed HTML/CSS into pixels: style cascade, layout
# (block/inline + flex + grid), text shaping, a CPU rasterizer, and an
# event/hit-test surface for game interaction. gold::game adds a thin
# adapter (uiSurface) on top of this; the module itself has no GPU
# dependency so it can be tested headless.
add_library(
	goldUI
	SHARED
		src/ui/font.cpp
		src/ui/layout.cpp
		src/ui/raster.cpp
		src/ui/renderer.cpp
		src/ui/style.cpp
		src/ui/tree.cpp
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
