cmake_minimum_required(VERSION 3.16...4.2)

project(gold CXX)

# The web module: transport-independent server facade (routes are
# buffered; start() resolves a loadable serverTransport plugin), the
# file-backed dataStore, and the HTML/CSS helpers. The transports
# themselves (the uWS stopgap, then libwebsockets) are plugins.
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

# The uWS transport: the HTTP stopgap plugin. Header-only uWS templates
# instantiate here (and only here), compiled against the vendored
# submodule until the libwebsockets port lands.
add_library(
	goldUws
	SHARED
		src/web/transportUws.cpp
)
add_library(
	gold::uws ALIAS goldUws
)
set_target_properties(
	goldUws
	PROPERTIES
		OUTPUT_NAME libgoldUws
		PREFIX ""
		LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
target_compile_options(goldUws PRIVATE -Wall -Wextra -pedantic)
target_include_directories(
	goldUws
	PRIVATE
		"include"
		"include/web"
		3rdParty/uWebSockets/src
)
target_link_libraries(
	goldUws
	PRIVATE
		gold::web
		uSockets
)
target_compile_features(goldUws PRIVATE cxx_std_26)