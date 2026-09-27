cmake_minimum_required(VERSION 3.16...4.2)

project(gold CXX)

add_library(
	goldLang
	SHARED
		src/lang/lang.cpp
		src/lang/lexer.cpp
		src/lang/parser.cpp
		src/lang/interpreter.cpp
)

add_library(
	gold::lang ALIAS goldLang
)

set_target_properties(
	goldLang
	PROPERTIES
		VERSION 0.1.0
		SOVERSION 0
)

if(MSVC)
  target_compile_options(goldLang PRIVATE /W4 -Wno-unused-function -Wno-unused-variable)
else()
  target_compile_options(goldLang PRIVATE -Wall -Wextra -pedantic -Wno-unused-function -Wno-unused-variable)
endif()

target_include_directories(
	goldLang
	PUBLIC
		"include"
		"include/lang"
)

target_link_libraries (
	goldLang
	PUBLIC 
		gold::shared
)

target_compile_features(
	goldLang
	PUBLIC
		cxx_std_26
)
