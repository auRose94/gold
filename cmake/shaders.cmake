
if(GOLD_USE_SYSTEM_BGFX)
	include(cmake/shadercParse.cmake)
endif()

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

set(SHADERS "" PARENT_SCOPE)
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

function ( read_to_hex_const PATH NAME OUTOUT DEPENDS)

	get_filename_component( FILENAME "${PATH}" NAME_WE )
	string( SUBSTRING "${FILENAME}" 0 2 TYPE )
	set( REQUIRED_DEFINE false )
	if( "${TYPE}" STREQUAL "fs" )
		set( REQUIRED_DEFINE true )
	elseif( "${TYPE}" STREQUAL "vs" )
		set( REQUIRED_DEFINE true )
	endif()

	if(EXISTS ${PATH})
		#message(STATUS "inlining ${PATH} at ${NAME}")
		file(READ ${PATH} DATA_READ HEX)
		string(REGEX REPLACE "(.)(.)"
       "0x\\1\\2, " DATA_FIXED
       ${DATA_READ})
		set(DATA ${DATA_FIXED})
		list(APPEND DEPENDS ${PATH} )
	else()
		set(DATA "")
	endif()
	string(LENGTH DATA DATA_SIZE)
	if(${REQUIRED_DEFINE} OR EXISTS ${PATH}) 
		if (DATA STREQUAL "")		
			set(${OUTOUT} "${${OUTOUT}}\nstatic const std::vector<uint8_t> ${NAME} = { 0x00, };\n" PARENT_SCOPE)
		else()
			set(${OUTOUT} "${${OUTOUT}}\nstatic const std::vector<uint8_t> ${NAME} = { ${DATA}0x00, };\n" PARENT_SCOPE)
		endif()
	endif()
endfunction()

function( inline_shader NAME HEADER BUFFER)
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
	
	set( ${BUFFER} "//THIS FILE IS GENERATED;")
	set( DEPENDS "")

	# dx9
	set( DX9_FRAG_OUTPUT ${SHADERS_ROOT}/dx9/${FRAG_NAME}.bin )
	set( DX9_VERT_OUTPUT ${SHADERS_ROOT}/dx9/${VERT_NAME}.bin )
	read_to_hex_const(${DX9_FRAG_OUTPUT} "dx9_fs_${NAME}" ${BUFFER} DEPENDS)
	read_to_hex_const(${DX9_VERT_OUTPUT} "dx9_vs_${NAME}" ${BUFFER} DEPENDS)

	# dx11
	set( DX11_FRAG_OUTPUT ${SHADERS_ROOT}/dx11/${FRAG_NAME}.bin )
	set( DX11_VERT_OUTPUT ${SHADERS_ROOT}/dx11/${VERT_NAME}.bin )
	set( DX11_COMP_OUTPUT ${SHADERS_ROOT}/dx11/${COMP_NAME}.bin )
	read_to_hex_const(${DX11_FRAG_OUTPUT} "dx11_fs_${NAME}" ${BUFFER} DEPENDS)
	read_to_hex_const(${DX11_VERT_OUTPUT} "dx11_vs_${NAME}" ${BUFFER} DEPENDS)
	read_to_hex_const(${DX11_COMP_OUTPUT} "dx11_cs_${NAME}" ${BUFFER} DEPENDS)

	# metal
	set( METAL_FRAG_OUTPUT ${SHADERS_ROOT}/metal/${FRAG_NAME}.bin )
	set( METAL_VERT_OUTPUT ${SHADERS_ROOT}/metal/${VERT_NAME}.bin )
	set( METAL_COMP_OUTPUT ${SHADERS_ROOT}/metal/${COMP_NAME}.bin )
	read_to_hex_const(${METAL_FRAG_OUTPUT} "metal_fs_${NAME}" ${BUFFER} DEPENDS)
	read_to_hex_const(${METAL_VERT_OUTPUT} "metal_vs_${NAME}" ${BUFFER} DEPENDS)
	read_to_hex_const(${METAL_COMP_OUTPUT} "metal_cs_${NAME}" ${BUFFER} DEPENDS)

	# essl
	set( ESSL_FRAG_OUTPUT ${SHADERS_ROOT}/essl/${FRAG_NAME}.bin )
	set( ESSL_VERT_OUTPUT ${SHADERS_ROOT}/essl/${VERT_NAME}.bin )
	set( ESSL_COMP_OUTPUT ${SHADERS_ROOT}/essl/${COMP_NAME}.bin )
	read_to_hex_const(${ESSL_FRAG_OUTPUT} "essl_fs_${NAME}" ${BUFFER} DEPENDS)
	read_to_hex_const(${ESSL_VERT_OUTPUT} "essl_vs_${NAME}" ${BUFFER} DEPENDS)
	read_to_hex_const(${ESSL_COMP_OUTPUT} "essl_cs_${NAME}" ${BUFFER} DEPENDS)

	# glsl
	set( GLSL_FRAG_OUTPUT ${SHADERS_ROOT}/glsl/${FRAG_NAME}.bin )
	set( GLSL_VERT_OUTPUT ${SHADERS_ROOT}/glsl/${VERT_NAME}.bin )
	set( GLSL_COMP_OUTPUT ${SHADERS_ROOT}/glsl/${COMP_NAME}.bin )
	read_to_hex_const(${GLSL_FRAG_OUTPUT} "glsl_fs_${NAME}" ${BUFFER} DEPENDS)
	read_to_hex_const(${GLSL_VERT_OUTPUT} "glsl_vs_${NAME}" ${BUFFER} DEPENDS)
	read_to_hex_const(${GLSL_COMP_OUTPUT} "glsl_cs_${NAME}" ${BUFFER} DEPENDS)

	# spirv
	set( SPIRV_FRAG_OUTPUT ${SHADERS_ROOT}/spirv/${FRAG_NAME}.bin )
	set( SPIRV_VERT_OUTPUT ${SHADERS_ROOT}/spirv/${VERT_NAME}.bin )
	set( SPIRV_COMP_OUTPUT ${SHADERS_ROOT}/spirv/${COMP_NAME}.bin )
	read_to_hex_const(${SPIRV_FRAG_OUTPUT} "spirv_fs_${NAME}" ${BUFFER} DEPENDS)
	read_to_hex_const(${SPIRV_VERT_OUTPUT} "spirv_vs_${NAME}" ${BUFFER} DEPENDS)
	read_to_hex_const(${SPIRV_COMP_OUTPUT} "spirv_cs_${NAME}" ${BUFFER} DEPENDS)

	if(EXISTS ${HEADER})
		file(READ ${HEADER} CURRENT_CONTENT)
		string(COMPARE NOTEQUAL "${${BUFFER}}" CURRENT_CONTENT SHOULD_UPDATE)
		file(GENERATE OUTPUT ${HEADER} CONTENT "${${BUFFER}}" CONDITION ${SHOULD_UPDATE})
	else()
		file(WRITE ${HEADER} "${${BUFFER}}")
	endif()
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


set(spriteBuffer "" PARENT_SCOPE)
inline_shader(
	"sprite" 
	"${CMAKE_CURRENT_BINARY_DIR}/shaderSprite.hpp" 
	spriteBuffer)

set(wfBuffer "" PARENT_SCOPE)
inline_shader(
	"wireframe" 
	"${CMAKE_CURRENT_BINARY_DIR}/shaderWireframe.hpp" 
	wfBuffer)