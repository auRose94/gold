cmake_minimum_required(VERSION 3.16...4.2)

project(LangExample)

add_executable(
	LangExample
	src/main.cpp
)

if(MSVC)
  target_compile_options(LangExample PRIVATE /W4)
else()
  target_compile_options(LangExample PRIVATE -Wall -Wextra -pedantic)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 26)

target_link_libraries(
	LangExample
	PRIVATE
		gold::lang
)

target_compile_features(
	LangExample
	PUBLIC
		cxx_std_26
)
