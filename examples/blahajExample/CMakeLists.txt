cmake_minimum_required(VERSION 3.16...4.2)

project(BlahajExample)

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

add_executable(BlahajExample src/main.cpp)
target_compile_features(BlahajExample PRIVATE cxx_std_26)
target_compile_options(BlahajExample PRIVATE -Wall -Wextra -pedantic)
target_include_directories(BlahajExample PRIVATE "${GOLD_SOURCE_DIR}/include")
target_link_libraries(BlahajExample PRIVATE gold::game)
