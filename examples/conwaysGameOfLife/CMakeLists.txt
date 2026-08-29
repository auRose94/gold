cmake_minimum_required(VERSION 3.16...4.2)

project(ConwaysGameOfLife)

include_directories(include)

get_filename_component(
	BUILD_ROOT
	"${CMAKE_CURRENT_BINARY_DIR}/"
	ABSOLUTE
)


get_filename_component(
	ASSETS_ABS
	"./assets/"
	ABSOLUTE
)

if(NOT (IS_SYMLINK ${BUILD_ROOT}/assets))
	file(
		CREATE_LINK 
		${ASSETS_ABS} 
		${BUILD_ROOT}/assets
		SYMBOLIC
	)
endif()

add_executable(
	ConwaysGameOfLife
	src/main.cpp
)

if(MSVC)
  target_compile_options(ConwaysGameOfLife PRIVATE /W4)
else()
  target_compile_options(ConwaysGameOfLife PRIVATE -Wall -Wextra -pedantic -Werror)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)

target_include_directories(
	ConwaysGameOfLife 
	PUBLIC
		"include"
)

target_link_libraries (
	ConwaysGameOfLife 
	PRIVATE
		gold::game
)

SET_TARGET_PROPERTIES(ConwaysGameOfLife PROPERTIES DEPLOYMENT_REMOTE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

target_compile_features(ConwaysGameOfLife
	PUBLIC
		cxx_variadic_templates
		cxx_nullptr
		cxx_generic_lambdas
		cxx_lambdas
		cxx_auto_type
		cxx_variable_templates
		cxx_variadic_macros
		cxx_template_template_parameters
		cxx_std_20
)

link_symbolic_shaders(${CMAKE_CURRENT_BINARY_DIR})
