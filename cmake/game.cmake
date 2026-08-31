
cmake_minimum_required(VERSION 3.16...4.2)

project(gold CXX)

add_library(
	goldGame
	SHARED
		shaderSprite.hpp
		src/game/audioSystem.cpp
		src/game/audioSystemSDL.cpp
		src/game/boxShape.cpp
		src/game/camera.cpp
		src/game/component.cpp
		src/game/engine.cpp
		src/game/entity.cpp
		src/game/envMap.cpp
		src/game/graphics.cpp
		src/game/inputSystem.cpp
		src/game/inputSystemEvdev.cpp
		src/game/inputSystemSDL.cpp
		src/game/light.cpp
		src/game/mesh.cpp
		src/game/meshRenderer.cpp
		src/game/meshShape.cpp
		src/game/physicsBody.cpp
		src/game/renderBackend.cpp
		src/game/renderBackendSDL.cpp
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
		${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/generated/wayland
		3rdParty/bullet3/src
)

if(NOT GOLD_USE_SYSTEM_BGFX)
	target_include_directories(
		goldGame
		PUBLIC
			${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/bgfx.cmake/bgfx/include
	)
endif()

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
	pkg_check_modules(SDL3 QUIET IMPORTED_TARGET sdl3)
	pkg_check_modules(WAYLAND_CLIENT QUIET IMPORTED_TARGET wayland-client)
	pkg_check_modules(WAYLAND_EGL QUIET IMPORTED_TARGET wayland-egl)
	pkg_check_modules(LIBEVDEV QUIET IMPORTED_TARGET libevdev)
endif()
if(NOT SDL3_FOUND)
	message(FATAL_ERROR "gold::game requires SDL3 (sdl3 pkg-config module)")
endif()

target_link_libraries (
	goldGame 
	PUBLIC 
		gold::shared
		${GOLD_BGFX_TARGET}
		Bullet3Common
		BulletSoftBody 
		BulletDynamics 
		BulletCollision 
		BulletInverseDynamicsUtils 
		BulletInverseDynamics 
		LinearMath
		PkgConfig::SDL3
		${OPENGL_LIBRARIES}
)
if(PkgConfig_FOUND AND WAYLAND_CLIENT_FOUND)
	target_link_libraries(goldGame PUBLIC PkgConfig::WAYLAND_CLIENT)
endif()
if(PkgConfig_FOUND AND WAYLAND_EGL_FOUND)
	target_link_libraries(goldGame PUBLIC PkgConfig::WAYLAND_EGL)
endif()
if(PkgConfig_FOUND AND LIBEVDEV_FOUND)
	target_link_libraries(goldGame PUBLIC PkgConfig::LIBEVDEV)
endif()
target_link_directories(goldGame PUBLIC ${LIBRARY_OUTPUT_DIRECTORY})

# Mirror the Bullet build configuration so goldGame compiles Bullet
# headers with the same ABI as the Bullet static libraries. The bundled
# Bullet CMake enables double precision (BT_USE_DOUBLE_PRECISION); without
# this define the sizes of btScalar/btVector3 differ between goldGame and
# the lib, corrupting the heap on any btDbvtBroadphase/btDiscreteDynamicsWorld
# construction.
target_compile_definitions(goldGame PUBLIC BT_USE_DOUBLE_PRECISION)

# In system-bgfx mode the shader compiler is the external bgfx-shaderc
# tool instead of the statically linked brtshaderc library.
if(GOLD_USE_SYSTEM_BGFX)
	target_compile_definitions(goldGame PUBLIC GOLD_USE_SYSTEM_BGFX=1)
endif()

# The shader compiler tool path, used at runtime to compile .sc shaders.
target_compile_definitions(goldGame PUBLIC
	GOLD_SHADER_COMPILER="${GOLD_SHADER_COMPILER}")

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
		cxx_std_26
)
