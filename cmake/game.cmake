
cmake_minimum_required(VERSION 3.16...4.2)

project(gold CXX)

add_library(
	goldGame
	SHARED
		shaderSprite.hpp
		src/game/audioSystem.cpp
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
		src/game/mcpServerSystem.cpp
		src/game/mesh.cpp
		src/game/meshRenderer.cpp
		src/game/meshShape.cpp
		src/game/physicsBackend.cpp
		src/game/physicsBody.cpp
		src/game/renderBackend.cpp
		src/game/renderable.cpp
		src/game/shape.cpp
		src/game/sphereShape.cpp
		src/game/sprite.cpp
		src/game/transform.cpp
		src/game/uiSurface.cpp
		src/game/window.cpp
		src/game/windowSystem.cpp
		src/game/windowSystemHeadless.cpp
		src/game/windowSystemWayland.cpp
		3rdParty/generated/wayland/xdg-shell-protocol.c
		src/game/world.cpp
)
add_dependencies(goldGame Shaders shaderEmbed)
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
		# Genex-wrapped for the export (see shared.cmake). The generated
		# shader/wayland dirs are build-tree-only.
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include/game>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/3rdParty/generated/wayland>
		$<INSTALL_INTERFACE:include>
		$<INSTALL_INTERFACE:include/game>
)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
	# Unconditional probes: results are cached, and skipping the call on a
	# cached found-flag would skip the imported-target creation too.
	pkg_check_modules(SDL3 QUIET IMPORTED_TARGET sdl3)
	pkg_check_modules(WAYLAND_CLIENT QUIET IMPORTED_TARGET wayland-client)
	pkg_check_modules(WAYLAND_EGL QUIET IMPORTED_TARGET wayland-egl)
	pkg_check_modules(LIBEVDEV QUIET IMPORTED_TARGET libevdev)
endif()
if(SDL3_FOUND)
	add_library(
		goldSDL3
		SHARED
			src/game/audioSystemSDL.cpp
			src/game/inputSystemSDL.cpp
			src/game/renderBackendSDL.cpp
			src/game/windowSystemSDL.cpp
	)
	add_library(
		gold::sdl3 ALIAS goldSDL3
	)
	set_target_properties(
		goldSDL3
		PROPERTIES
			OUTPUT_NAME libgoldSdl3
			PREFIX ""
			LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
	)
	target_compile_options(goldSDL3 PRIVATE -Wall -Wextra -pedantic)
	target_include_directories(
		goldSDL3
		PRIVATE
			"include"
			"include/game"
	)
	# The SDL3 plugin also carries the SDL_GPU render backend; shader
	# compilation is a backend concern, so the tool paths bake in here
	# the same way the bgfx plugin's do.
	target_compile_definitions(goldSDL3 PRIVATE
		GOLD_SHADER_COMPILER="${GOLD_SHADER_COMPILER}"
		GOLD_BGFX_SHADER_INCLUDE="${GOLD_BGFX_SHADER_INCLUDE}")
	# The SDL window backend acquires EGL surfaces when SDL runs on
	# Wayland, so the plugin carries that interop too.
	target_link_libraries(
		goldSDL3
		PRIVATE
			gold::game
			PkgConfig::SDL3
			# bx's matrix helpers (the per-draw uniform packets) + bimg's
			# PNG writer (the backend's own screenshot path).
			${GOLD_BX_TARGET}
			${GOLD_BIMG_TARGET}
	)
	target_compile_features(goldSDL3 PRIVATE cxx_std_26)
	if(PkgConfig_FOUND AND WAYLAND_EGL_FOUND)
		target_link_libraries(goldSDL3 PRIVATE PkgConfig::WAYLAND_EGL)
	endif()
else()
	message(STATUS "gold: system SDL3 not found; building gold::game without the SDL backends (headless/wayland/evdev only)")
endif()

# The SDL2 parity adapter: when only SDL2 exists, windows/input/audio
# still work (rendering falls to bgfx — SDL2 has no SDL_GPU). Built only
# when the system sdl2 development headers are present; the alias chain
# "sdl" probes sdl3 first, then sdl2.
if(PkgConfig_FOUND)
	pkg_check_modules(SDL2 QUIET IMPORTED_TARGET sdl2)
	if(SDL2_FOUND)
		add_library(
			goldSDL2
			SHARED
				src/game/audioSystemSdl2.cpp
				src/game/inputSystemSdl2.cpp
				src/game/windowSystemSdl2.cpp
		)
		add_library(
			gold::sdl2 ALIAS goldSDL2
		)
		set_target_properties(
			goldSDL2
			PROPERTIES
				OUTPUT_NAME libgoldSdl2
				PREFIX ""
				LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
		)
		target_compile_options(goldSDL2 PRIVATE -Wall -Wextra -pedantic)
		target_include_directories(
			goldSDL2
			PRIVATE
				"include"
				"include/game"
		)
		target_link_libraries(
			goldSDL2
			PRIVATE
				gold::game
				PkgConfig::SDL2
			)
		target_compile_features(goldSDL2 PRIVATE cxx_std_26)
		if(PkgConfig_FOUND AND WAYLAND_EGL_FOUND)
			# The SDL2 window backend creates wl_egl_windows the same way.
			target_link_libraries(goldSDL2 PRIVATE PkgConfig::WAYLAND_EGL)
		endif()
	else()
		message(STATUS "gold: system SDL2 not found; the sdl2 parity adapter is not built")
	endif()
endif()

target_link_libraries (
	goldGame
	PUBLIC
		gold::shared
		${OPENGL_LIBRARIES}
	PRIVATE
		# The facade's bx math/timing and bimg container decode stay;
		# the renderer itself does not: libgoldBgfx (the loadable bgfx
		# plugin) is the only library linked to libbgfx.
		${GOLD_BX_TARGET}
		${GOLD_BIMG_TARGET}
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

# gold::ui backs the uiSurface component (HTML/CSS rendered to a texture).
# The include dir is PRIVATE: the header is the only thing game code needs.
if(TARGET gold::ui)
	target_link_libraries(goldGame PRIVATE gold::ui)
endif()

# No shader-tool constants bake into the facade: shader compilation is
# a backend concern (the plugins define GOLD_SHADER_COMPILER and the
# include dir themselves) — gfxBackend and shaderObject only ask the
# active backend to compile stages.

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

# The bullet physics backend is a loadable plugin compiled against the
# system bullet package (use the double-precision build, `bullet-dp` on
# Arch — gold mirrors BT_USE_DOUBLE_PRECISION on both sides so the ABIs
# agree; mismatched precision corrupts the heap on btVector3 sizing).
# The world/shape/body facades in libgoldGame stay bt-free and fall back
# to the built-in no-op "none" backend without it.
find_package(Bullet)
if(Bullet_FOUND)
	add_library(
		goldBullet
		SHARED
			src/physics/bulletPhysicsBackend.cpp
	)
	add_library(
		gold::bullet ALIAS goldBullet
	)
	set_target_properties(
		goldBullet
		PROPERTIES
			OUTPUT_NAME libgoldBullet
			PREFIX ""
			LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
	)
	target_compile_options(goldBullet PRIVATE -Wall -Wextra -pedantic)
	target_include_directories(
		goldBullet
		PRIVATE
			"include"
			"include/game"
			${BULLET_INCLUDE_DIRS}
	)
	target_link_libraries(
		goldBullet
		PRIVATE
			gold::game
			${BULLET_LIBRARIES}
	)
	target_compile_definitions(goldBullet PRIVATE BT_USE_DOUBLE_PRECISION)
	target_compile_features(goldBullet PRIVATE cxx_std_26)
else()
	message(STATUS "gold: system bullet not found; physics runs the no-op \"none\" backend (install bullet-dp or a double-precision bullet build for simulation)")
endif()

# The bgfx render backend is a loadable plugin: compiled against the
# system bgfx/bimg/bx headers and linked to their libraries PRIVATEly,
# self-registering through static initializers. libgoldGame carries no
# bgfx code; createRenderBackend("bgfx") finds it via the plugin loader
# ("bgfx" -> libgoldBgfx.so, searched GOLD_PLUGIN_PATH -> exe dir ->
# module dirs).
if(bgfx_FOUND)
	add_library(
		goldBgfx
		SHARED
			src/game/renderBackendBgfx.cpp
	)
	add_library(
		gold::bgfx ALIAS goldBgfx
	)
	set_target_properties(
		goldBgfx
		PROPERTIES
			OUTPUT_NAME libgoldBgfx
			PREFIX ""
			LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
	)
	target_compile_options(goldBgfx PRIVATE -Wall -Wextra -pedantic)
	target_include_directories(
		goldBgfx
		PRIVATE
			"include"
			"include/game"
	)
	target_link_libraries(
		goldBgfx
		PRIVATE
			gold::game
			${GOLD_BGFX_TARGET}
			${GOLD_BIMG_TARGET}
			${GOLD_BX_TARGET}
	)
	target_compile_features(goldBgfx PRIVATE cxx_std_26)
	# The runtime compile path (shaderObject's "src") shells the compiler
	# tool from THIS plugin; only backends bake tool-path constants.
	target_compile_definitions(goldBgfx PRIVATE
		GOLD_SHADER_COMPILER="${GOLD_SHADER_COMPILER}"
		GOLD_BGFX_SHADER_INCLUDE="${GOLD_BGFX_SHADER_INCLUDE}")
endif()
