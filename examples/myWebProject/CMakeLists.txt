cmake_minimum_required(VERSION 3.10)

# set the project name
project(MyWebProject)

include_directories(include)
add_subdirectory(3rdParty)

get_filename_component(
	BUILD_ROOT
	"${CMAKE_CURRENT_BINARY_DIR}/"
	ABSOLUTE
)

get_filename_component(
	JS_ABS
	"./js/"
	ABSOLUTE
)

get_filename_component(
	CSS_ABS
	"./css/"
	ABSOLUTE
)

# js/, css/ and their subfolders are symlinked into the build tree.
# ./assets is a writable upload/data directory: the server creates it at
# runtime (server::setMountPoint), so there is no symlink for it here.
if(NOT (IS_SYMLINK ${BUILD_ROOT}/js/main))
	file(
		CREATE_LINK 
		${JS_ABS} 
		${BUILD_ROOT}/js/main
		SYMBOLIC
	)
endif()

if(NOT (IS_SYMLINK ${BUILD_ROOT}/css/main))
	file(
		CREATE_LINK 
		${CSS_ABS} 
		${BUILD_ROOT}/css/main
		SYMBOLIC
	)
endif()

# add the executable
add_executable(
	MyWebProject
	src/routes/upload/find.cpp
	src/routes/upload/index.cpp
	src/routes/upload/list.cpp
	src/routes/upload/options.cpp
	src/routes/user/home.cpp
	src/routes/user/login.cpp
	src/routes/user/options.cpp
	src/routes/user/register.cpp
	src/routes/index.cpp
	src/bootstrap.cpp
	src/error.cpp
	src/navbar.cpp
	src/main.cpp
	src/session.cpp
	src/template.cpp
	src/upload.cpp
	src/user.cpp
	src/utility.cpp
)

if(MSVC)
  target_compile_options(MyWebProject PRIVATE /W4)
else()
  target_compile_options(MyWebProject PRIVATE -Wall -pedantic)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)

SET_TARGET_PROPERTIES(MyWebProject PROPERTIES DEPLOYMENT_REMOTE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

target_include_directories(
	MyWebProject
	PUBLIC
		"include"
)

target_link_libraries(
	MyWebProject 
	PRIVATE
		gold::web
)

target_compile_features(
	MyWebProject
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