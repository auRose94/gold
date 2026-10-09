cmake_minimum_required(VERSION 3.16...4.2)

project(gold CXX)

# The web module: transport-independent server facade (routes are
# buffered; start() resolves a loadable serverTransport plugin), the
# file-backed dataStore, and the HTML/CSS helpers. The transport
# itself (libwebsockets) is a plugin.
add_library(
	goldWeb
	SHARED
		src/web/css.cpp
		src/web/database.cpp
		src/web/dataStoreFile.cpp
		src/web/html.cpp
		src/web/server.cpp
		src/web/serverTransport.cpp
)

add_library(
	gold::web ALIAS goldWeb
)

set_target_properties(
	goldWeb
	PROPERTIES
		VERSION 0.1.0
		SOVERSION 0
)

if(MSVC)
  target_compile_options(goldWeb PRIVATE /W4 -Wno-unused-function -Wno-unused-variable)
else()
  target_compile_options(goldWeb PRIVATE -Wall -Wextra -pedantic -Wno-unused-function -Wno-unused-variable)
endif()

target_include_directories(
	goldWeb
	PUBLIC
		"include"
		"include/web"
)

target_link_libraries (
	goldWeb
	PUBLIC
		gold::shared
)

target_compile_features(
	goldWeb
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

# The libwebsockets transport: gold's real HTTP/WebSocket engine, built
# only when the system package exists (graceful degradation otherwise —
# the server facade reports "no server transport available" at start).
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
	# Unconditional call: results are cached; a skipped call on a cached
	# found-flag would skip the imported-target creation too.
	pkg_check_modules(LWS QUIET IMPORTED_TARGET libwebsockets)
endif()
if(LWS_FOUND)
	add_library(
		goldLws
		SHARED
			src/web/transportLws.cpp
	)
	add_library(
		gold::lws ALIAS goldLws
	)
	set_target_properties(
		goldLws
		PROPERTIES
			OUTPUT_NAME libgoldLws
			PREFIX ""
			LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
	)
	target_compile_options(goldLws PRIVATE -Wall -Wextra -pedantic)
	target_include_directories(
		goldLws
		PRIVATE
			"include"
			"include/web"
			${LWS_INCLUDE_DIRS}
	)
	target_link_libraries(
		goldLws
		PRIVATE
			gold::web
			PkgConfig::LWS
	)
	target_compile_features(goldLws PRIVATE cxx_std_26)
else()
	message(STATUS "gold: system libwebsockets not found; building without the lws server transport")
endif()