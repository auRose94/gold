cmake_minimum_required(VERSION 3.16...4.2)

project(gold CXX)

# The MCP module: gold's embeddable Model Context Protocol development
# server (a JSON-RPC "/mcp" endpoint over the web module's server
# facade, default transport "lws"). The game engine loads it on demand
# through the mcpServerSystem seam when the app's config asks for it
# ("mcp": {"enabled", true}); apps and tests may also link it directly.
# gold::lang powers the "eval" tool when built; without a web transport
# (no lws) the module has nothing to listen on and is not built at all.
add_library(
	goldMcp
	SHARED
		src/mcp/mcpDispatcher.cpp
		src/mcp/mcpServer.cpp
		src/mcp/mcpTools.cpp
)
add_library(
	gold::mcp ALIAS goldMcp
)

set_target_properties(
	goldMcp
	PROPERTIES
		VERSION 0.1.0
		SOVERSION 0
		OUTPUT_NAME libgoldMcp
		PREFIX ""
		LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

if(MSVC)
  target_compile_options(goldMcp PRIVATE /W4)
else()
  target_compile_options(goldMcp PRIVATE -Wall -Wextra -pedantic)
endif()

target_include_directories(
	goldMcp
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include/mcp>
		$<INSTALL_INTERFACE:include>
		$<INSTALL_INTERFACE:include/mcp>
)

target_link_libraries(
	goldMcp
	PUBLIC
		# The facade's includes: gold::web headers (the server facade)
		# stay visible to consumers that drive the endpoint directly.
		gold::shared
		gold::web
	PRIVATE
		# The seam interface + registration (the engine module is only
		# consumed as a plugin target, like the other backends).
		gold::game
)

target_compile_features(
	goldMcp
	PRIVATE
		cxx_std_26
)

# The "eval" tool needs gold::lang; without the module the catalog just
# ships without it.
if(TARGET goldLang)
	target_link_libraries(goldMcp PRIVATE gold::lang)
	target_compile_definitions(goldMcp PRIVATE GOLD_MCP_LANG=1)
endif()