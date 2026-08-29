
cmake_minimum_required(VERSION 3.16...4.2)

project(gold CXX)

add_library(
	goldWeb
	SHARED
		src/web/database.cpp
		src/web/dataStoreFile.cpp
		src/web/html.cpp
		src/web/server.cpp
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
  target_compile_options(goldWeb PRIVATE /W4)
else()
  target_compile_options(goldWeb PRIVATE -pedantic)
endif()

target_include_directories(
	goldWeb
	PUBLIC
		"include"
		"include/web"
		3rdParty/uWebSockets/src
)

target_link_libraries (
	goldWeb
	PUBLIC 
		gold::shared
		uWebSockets
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
