
cmake_minimum_required(VERSION 3.10)

project(gold CXX)

add_library(
	goldGame
	SHARED
		shaderSprite.hpp
		src/game/boxShape.cpp
		src/game/camera.cpp
		src/game/component.cpp
		src/game/engine.cpp
		src/game/entity.cpp
		src/game/envMap.cpp
		src/game/graphics.cpp
		src/game/inputSystem.cpp
		src/game/inputSystemEvdev.cpp
		src/game/light.cpp
		src/game/mesh.cpp
		src/game/meshRenderer.cpp
		src/game/meshShape.cpp
		src/game/physicsBody.cpp
		src/game/renderBackend.cpp
		src/game/renderable.cpp
		src/game/shape.cpp
		src/game/sphereShape.cpp
		src/game/sprite.cpp
		src/game/transform.cpp
		src/game/window.cpp
		src/game/windowSystem.cpp
		src/game/windowSystemHeadless.cpp
		src/game/windowSystemSDL.cpp
		src/game/windowSystemWayland.cpp
		3rdParty/generated/wayland/xdg-shell-protocol.c
		src/game/world.cpp
)
add_dependencies(goldGame Shaders)
add_library(
	gold::game ALIAS goldGame
)

set_target_properties(
	goldGame
	PROPERTIES
		VERSION 0.1.0
		SOVERSION 0
)

if(MSVC)
  target_compile_options(goldGame PRIVATE /W4)
else()
  target_compile_options(goldGame PRIVATE -Wall -Wextra -pedantic)
endif()

target_include_directories(
	goldGame
	PUBLIC
		"include"
		"include/game"
		${CMAKE_CURRENT_BINARY_DIR}
		${CMAKE_CURRENT_BINARY_DIR}/3rdParty/SDL/include
		${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/generated/wayland
		3rdParty/bullet3/src
)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
	pkg_check_modules(WAYLAND_CLIENT QUIET IMPORTED_TARGET wayland-client)
	pkg_check_modules(LIBEVDEV QUIET IMPORTED_TARGET libevdev)
endif()

target_link_libraries (
	goldGame 
	PUBLIC 
		gold::shared
		brtshaderc
		bgfx
		Bullet3Common
		BulletSoftBody 
		BulletDynamics 
		BulletCollision 
		BulletInverseDynamicsUtils 
		BulletInverseDynamics 
		LinearMath
		SDL2-static
		${OPENGL_LIBRARIES}
)
if(PkgConfig_FOUND AND WAYLAND_CLIENT_FOUND)
	target_link_libraries(goldGame PUBLIC PkgConfig::WAYLAND_CLIENT)
endif()
if(PkgConfig_FOUND AND LIBEVDEV_FOUND)
	target_link_libraries(goldGame PUBLIC PkgConfig::LIBEVDEV)
endif()
target_link_directories(goldGame PUBLIC ${LIBRARY_OUTPUT_DIRECTORY})

# SDL ships a minimal SDL_config.h in its source tree that shadows the
# CMake-generated one (which enables the X11/Wayland syswm backends).
# Force the X11 backend here so SDL_syswm.h exposes the x11 union member.
if(UNIX AND NOT APPLE)
	target_compile_definitions(goldGame PUBLIC SDL_VIDEO_DRIVER_X11=1)
endif()

target_compile_features(
	goldGame
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