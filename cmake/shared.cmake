
cmake_minimum_required(VERSION 3.16...4.2)

project(gold CXX)

add_library(
	goldShared
	STATIC
		src/list.cpp
		src/file.cpp
		src/goldjson.cpp
		src/image.cpp
		src/module.cpp
		src/object.cpp
		src/plugin.cpp
		src/types.cpp
		src/var.cpp
		src/promise.cpp
)
add_library(
	gold::shared ALIAS goldShared
)

if(MSVC)
  target_compile_options(goldShared PRIVATE /W4 -Wno-unused-function -Wno-unused-variable)
else()
  target_compile_options(goldShared PRIVATE -Wall -Wextra -pedantic -Wno-unused-function -Wno-unused-variable)
endif()

target_include_directories(
	goldShared
	PUBLIC
		"include"
)

target_link_libraries (
	goldShared
	PUBLIC 
		OpenSSL::Crypto
		bgfx::bx
		bgfx::bimg
)

if(MSVC)
else()
	if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
		# std::filesystem lived in libstdc++fs before GCC 9.
		target_link_libraries(
			goldShared
			PUBLIC
				stdc++fs
		)
	endif()
	# module.cpp uses dlopen/dlclose for optional module loading.
	find_package(Threads REQUIRED)
	target_link_libraries(goldShared PRIVATE ${CMAKE_DL_LIBS})
endif(MSVC)

target_compile_features(
	goldShared
	PUBLIC
		cxx_variadic_templates
		cxx_nullptr
		cxx_generic_lambdas
		cxx_lambdas
		cxx_lambda_init_captures
		cxx_unrestricted_unions
		cxx_return_type_deduction
		cxx_local_type_template_args
		cxx_rvalue_references
		cxx_alias_templates
		cxx_static_assert
		cxx_auto_type
		cxx_variable_templates
		cxx_variadic_macros
		cxx_template_template_parameters
		cxx_std_26
)
