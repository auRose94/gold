# gold's shaderc build recipe.
#
# This mirrors bgfx.cmake's cmake/bgfx/shaderc.cmake but lives in gold's own
# tree so the shader-toolchain link fix (circular static archives) is
# committed with the repo instead of living as an uncommitted edit inside the
# bgfx.cmake submodule. It reads the shaderc sources from the bgfx.cmake
# submodule via BGFX_DIR, exactly as the upstream recipe does.

# Grab the shaderc source files
file(
	GLOB
	SHADERC_SOURCES #
	${BGFX_DIR}/tools/shaderc/*.cpp #
	${BGFX_DIR}/tools/shaderc/*.h #
	${BGFX_DIR}/src/shader* #
)

add_executable(shaderc ${SHADERC_SOURCES})

target_link_libraries(
	shaderc
	PRIVATE bx
			bimg
			bgfx-vertexlayout
			spirv-cross
			webgpu
)

# The shader toolchain static archives (glslang, tint, spirv-opt) have
# circular references between them. Static archives resolve left-to-right,
# so a single pass cannot satisfy both directions. Wrap them in a link group
# so the linker iterates until all symbols resolve.
if(NOT MSVC)
	target_link_libraries(shaderc PRIVATE
		"-Wl,--start-group" glslang tint spirv-opt "-Wl,--end-group"
	)
else()
	target_link_libraries(shaderc PRIVATE glslang tint spirv-opt)
endif()

target_include_directories(
	shaderc
	PRIVATE ${BGFX_DIR}/3rdparty/dawn
			${BGFX_DIR}/3rdparty/dawn/src
)

set(DXCOMPILER_RUNTIME)
if(UNIX
   AND NOT APPLE
   AND NOT EMSCRIPTEN
   AND NOT ANDROID
)
	target_include_directories(
		shaderc
		PRIVATE ${BGFX_DIR}/3rdparty/directx-headers/include/directx
				${BGFX_DIR}/3rdparty/directx-headers/include
				${BGFX_DIR}/3rdparty/directx-headers/include/wsl/stubs
	)
	set(DXCOMPILER_RUNTIME ${BGFX_DIR}/tools/bin/linux/libdxcompiler.so)
elseif(WIN32)
	set(DXCOMPILER_RUNTIME ${BGFX_DIR}/tools/bin/windows/dxcompiler.dll)
endif()

if(BGFX_AMALGAMATED)
	target_link_libraries(shaderc PRIVATE bgfx-shader)
endif()

set_target_properties(
	shaderc PROPERTIES FOLDER "bgfx/tools" #
					   OUTPUT_NAME ${BGFX_TOOLS_PREFIX}shaderc #
)

if(BGFX_BUILD_TOOLS_SHADER)
	add_executable(bgfx::shaderc ALIAS shaderc)
	if(BGFX_CUSTOM_TARGETS)
		add_dependencies(bgfx-tools shaderc)
	endif()
endif()

if(ANDROID)
	target_link_libraries(shaderc PRIVATE log)
elseif(IOS)
	set_target_properties(shaderc PROPERTIES MACOSX_BUNDLE ON MACOSX_BUNDLE_GUI_IDENTIFIER shaderc)
endif()

if(BGFX_INSTALL)
	install(TARGETS shaderc EXPORT "${TARGETS_EXPORT_NAME}" DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()

# DXIL compiler will be dynamically loaded at runtime - no need
# to link, just install the needed binaries alongside shaderc.exe
if(DXCOMPILER_RUNTIME)
	add_custom_command(
		TARGET shaderc POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DXCOMPILER_RUNTIME} $<TARGET_FILE_DIR:shaderc>
	)
	if(BGFX_INSTALL)
		install(FILES ${DXCOMPILER_RUNTIME} DESTINATION "${CMAKE_INSTALL_BINDIR}")
	endif()
endif()
