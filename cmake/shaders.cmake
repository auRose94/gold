
include(cmake/shadercParse.cmake)

get_filename_component(
	SHADERS_ROOT
	"${CMAKE_CURRENT_BINARY_DIR}/bin"
	ABSOLUTE
)

FILE(GLOB_RECURSE GLOB_SHADERS  CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/shaders/*.sc)

set_source_files_properties(${SHADERS}
PROPERTIES GENERATED TRUE)

add_library(Shaders OBJECT)

set_target_properties(Shaders PROPERTIES LINKER_LANGUAGE CXX)

source_group( "Shader Files" FILES "${GLOB_SHADERS}")

set(SHADERS "")
function( add_bgfx_shader FILE RETURN)
	get_filename_component( FILENAME "${FILE}" NAME_WE )
	string( SUBSTRING "${FILENAME}" 0 2 TYPE )
	if( "${TYPE}" STREQUAL "fs" )
		set( TYPE "FRAGMENT" )
		set( D3D_PREFIX "ps" )
	elseif( "${TYPE}" STREQUAL "vs" )
		set( TYPE "VERTEX" )
		set( D3D_PREFIX "vs" )
	elseif( "${TYPE}" STREQUAL "cs" )
		set( TYPE "COMPUTE" )
 		set( D3D_PREFIX "cs" )
	else()
		set( TYPE "" )
	endif()

	if( NOT "${TYPE}" STREQUAL "" )
		set( COMMON FILE ${FILE} ${TYPE} INCLUDES ${GOLD_BGFX_SHADER_INCLUDE} )
		set( OUTPUTS "" )
		set( OUTPUTS_PRETTY "" )

		if( WIN32 )
			# dx9
			if( NOT "${TYPE}" STREQUAL "COMPUTE" )
				set( DX9_OUTPUT ${SHADERS_ROOT}/dx9/${FILENAME}.bin )
				shaderc_parse( DX9 ${COMMON} WINDOWS PROFILE ${D3D_PREFIX}_3_0 O 3 OUTPUT ${DX9_OUTPUT} )
				list( APPEND OUTPUTS "DX9" )
				set( OUTPUTS_PRETTY "${OUTPUTS_PRETTY}DX9, " )
			endif()

			# dx11
			set( DX11_OUTPUT ${SHADERS_ROOT}/dx11/${FILENAME}.bin )
			if( NOT "${TYPE}" STREQUAL "COMPUTE" )
				shaderc_parse( DX11 ${COMMON} WINDOWS PROFILE ${D3D_PREFIX}_5_0 O 3 OUTPUT ${DX11_OUTPUT} )
			else()
				shaderc_parse( DX11 ${COMMON} WINDOWS PROFILE ${D3D_PREFIX}_5_0 O 1 OUTPUT ${DX11_OUTPUT} )
			endif()
			list( APPEND OUTPUTS "DX11" )
			set( OUTPUTS_PRETTY "${OUTPUTS_PRETTY}DX11, " )
		endif()

		if( APPLE )
			# metal
			set( METAL_OUTPUT ${SHADERS_ROOT}/metal/${FILENAME}.bin )
			shaderc_parse( METAL ${COMMON} OSX PROFILE metal OUTPUT ${METAL_OUTPUT} )
			list( APPEND OUTPUTS "METAL" )
			set( OUTPUTS_PRETTY "${OUTPUTS_PRETTY}Metal, " )
		endif()

		# essl
		if( NOT "${TYPE}" STREQUAL "COMPUTE" )
			set( ESSL_OUTPUT ${SHADERS_ROOT}/essl/${FILENAME}.bin )
			shaderc_parse( ESSL ${COMMON} ANDROID PROFILE 300_es OUTPUT ${ESSL_OUTPUT} )
			list( APPEND OUTPUTS "ESSL" )
			set( OUTPUTS_PRETTY "${OUTPUTS_PRETTY}ESSL, " )
		endif()

		# glsl
		set( GLSL_OUTPUT ${SHADERS_ROOT}/glsl/${FILENAME}.bin )
		if( NOT "${TYPE}" STREQUAL "COMPUTE" )
			shaderc_parse( GLSL ${COMMON} LINUX PROFILE 330 OUTPUT ${GLSL_OUTPUT} )
		else()
			shaderc_parse( GLSL ${COMMON} LINUX PROFILE 430 OUTPUT ${GLSL_OUTPUT} )
		endif()
		list( APPEND OUTPUTS "GLSL" )
		set( OUTPUTS_PRETTY "${OUTPUTS_PRETTY}GLSL, " )

		# spirv
		if( NOT "${TYPE}" STREQUAL "COMPUTE" )
			set( SPIRV_OUTPUT ${SHADERS_ROOT}/spirv/${FILENAME}.bin )
			shaderc_parse( SPIRV ${COMMON} LINUX PROFILE spirv OUTPUT ${SPIRV_OUTPUT} )
			list( APPEND OUTPUTS "SPIRV" )
			set( OUTPUTS_PRETTY "${OUTPUTS_PRETTY}SPIRV" )
			set( OUTPUT_FILES "" )
			set( COMMANDS "" )
		endif()

		foreach( OUT ${OUTPUTS} )
			list( APPEND OUTPUT_FILES ${${OUT}_OUTPUT} )
			list( APPEND COMMANDS COMMAND "${GOLD_SHADER_COMPILER}" ${${OUT}} )
			get_filename_component( OUT_DIR ${${OUT}_OUTPUT} DIRECTORY )
			file( MAKE_DIRECTORY ${OUT_DIR} )
		endforeach()

		add_custom_command(
			MAIN_DEPENDENCY
			${FILE}
			OUTPUT
			${OUTPUT_FILES}
			${COMMANDS}
			COMMENT "Compiling shader ${FILENAME} for ${OUTPUTS_PRETTY}"
		)

		set(${RETURN} ${OUTPUT_FILES} PARENT_SCOPE)

	endif()
endfunction()

function( inline_shader NAME HEADER)
	set(VERT_SHADER "" PARENT_SCOPE)
	set(FRAG_SHADER "" PARENT_SCOPE)
	set(COMP_SHADER "" PARENT_SCOPE)

	set(VERT_NAME "vs_${NAME}")
	set(FRAG_NAME "fs_${NAME}")
	set(COMP_NAME "cs_${NAME}")

	set(VERT_PATH
		${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/${NAME}/${VERT_NAME}.sc)
	set(FRAG_PATH
		${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/${NAME}/${FRAG_NAME}.sc)
	set(COMP_PATH
		${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/${NAME}/${COMP_NAME}.sc)

	if(EXISTS ${VERT_PATH})
		add_bgfx_shader( ${VERT_PATH} VERT_SHADER)
		list(APPEND SHADERS ${VERT_SHADER})
	endif()
	if(EXISTS ${FRAG_PATH})
		add_bgfx_shader( ${FRAG_PATH} FRAG_SHADER)
		list(APPEND SHADERS ${FRAG_SHADER})
	endif()
	if(EXISTS ${COMP_PATH})
		add_bgfx_shader( ${COMP_PATH} COMP_SHADER)
		list(APPEND SHADERS ${COMP_SHADER})
	endif()

	target_sources(Shaders PUBLIC ${SHADERS})

	# Build-time embedding: a custom command turns each compiled .bin
	# into a byte-array constant in the target header. The old flow
	# embedded at configure time, baking {0x00} placeholders into fresh
	# builds until a second configure — the game rendered nothing.
	#
	# The embed emits EVERY consumer-visible (variant, kind) key; the
	# script writes a {0x00} placeholder for paths that exist only on
	# other platforms (dx11/metal on Linux, say), since consumer code
	# switches over all of them by name.
	if(EXISTS ${COMP_PATH})
		set(KINDS fs vs cs)
	else()
		set(KINDS fs vs)
	endif()
	set(EMBED_ENTRIES "")
	foreach(variant IN ITEMS dx9 dx11 metal essl glsl spirv)
		foreach(kind IN ITEMS ${KINDS})
			list(APPEND EMBED_ENTRIES
				"${variant}_${kind}_${NAME}|${SHADERS_ROOT}/${variant}/${kind}_${NAME}.bin")
		endforeach()
	endforeach()
	# The entries list carries semicolons that would be mangled when
	# passed as a -D value through make/ninja shells; hand it over via a
	# configure-time file instead (one "name|path" per line).
	string(JOIN "\n" entriesText ${EMBED_ENTRIES})
	file(WRITE
		"${CMAKE_CURRENT_BINARY_DIR}/${NAME}EmbedEntries.txt"
		"${entriesText}")
	add_custom_command(
		OUTPUT ${HEADER}
		COMMAND ${CMAKE_COMMAND}
			-DOUT=${HEADER}
			-DENTRIES_FILE=${CMAKE_CURRENT_BINARY_DIR}/${NAME}EmbedEntries.txt
			-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/shaderEmbedScript.cmake
		DEPENDS ${VERT_SHADER} ${FRAG_SHADER} ${COMP_SHADER}
		COMMENT "Embedding ${NAME} shaders into ${HEADER}"
	)
	set_property(
		GLOBAL APPEND PROPERTY GOLD_SHADER_EMBED_HEADERS ${HEADER})
endfunction()

function(link_symbolic_shaders BUILD_PATH)
	set(symbolicShaderNames "")
	list(APPEND symbolicShaderNames 
		common 
		pbr
	)
	# The examples are designed to live next to a gold submodule, so default
	# to ../gold. When built in-tree from the gold repo, GOLD_SOURCE_DIR is
	# set by the top-level CMakeLists and overrides the relative path.
	if(NOT DEFINED GOLD_SOURCE_DIR)
		set(GOLD_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../gold")
	endif()
	foreach( NAME ${symbolicShaderNames} )
		set(
			SOURCE_FOLDER
			"${GOLD_SOURCE_DIR}/src/shaders/${NAME}/"
		)
		if(NOT (EXISTS ${BUILD_PATH}/shaders))
			file(
				MAKE_DIRECTORY
				${BUILD_PATH}/shaders
			)
		endif()
		if(NOT (IS_SYMLINK ${BUILD_PATH}/shaders/${NAME}))
			file(
				CREATE_LINK 
				${SOURCE_FOLDER}
				${BUILD_PATH}/shaders/${NAME}
				SYMBOLIC
			)
		endif()
	endforeach()
endfunction()


inline_shader("sprite" "${CMAKE_CURRENT_BINARY_DIR}/shaderSprite.hpp")

inline_shader("wireframe" "${CMAKE_CURRENT_BINARY_DIR}/shaderWireframe.hpp")

# One buildable sink for both headers; goldGame orders itself behind it
# (add_dependencies in game.cmake), so its objects never compile against
# missing or stale embeds.
get_property(EMBED_HEADERS GLOBAL PROPERTY GOLD_SHADER_EMBED_HEADERS)
add_custom_target(shaderEmbed DEPENDS ${EMBED_HEADERS})
