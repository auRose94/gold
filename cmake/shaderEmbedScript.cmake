# Build-time shader embedding: invoked with -P by a custom command that
# runs after the shader binaries exist (the old configure-time embed
# baked {0x00} placeholders into libgoldGame on any fresh build — the
# game shipped without shaders until a second configure).
#
# Usage: cmake -DOUT=<header.hpp> -DENTRIES_FILE=<entries.txt>
#            -P shaderEmbedScript.cmake
# The entries file holds one "name|path" per line (CMake list format is
# fine too: semicolon- or newline-separated).

if(ENTRIES_FILE)
	file(STRINGS "${ENTRIES_FILE}" EMBED_ENTRIES)
endif()

set(HEADER_CONTENT "//THIS FILE IS GENERATED;")
foreach(entry IN LISTS EMBED_ENTRIES)
	string(REPLACE "|" ";" kv "${entry}")
	list(GET kv 0 name)
	list(GET kv 1 path)
	if(EXISTS ${path})
		# Embed only fully written artifacts.
		file(SIZE ${path} size)
		if(${size} GREATER 0)
			file(READ ${path} hex HEX)
			string(REGEX REPLACE "(.)(.)" "0x\\1\\2, " data "${hex}")
			string(APPEND HEADER_CONTENT
				"\nstatic const std::vector<uint8_t> ${name} = { ${data}0x00, };\n")
			continue()
		endif()
	endif()
	string(APPEND HEADER_CONTENT
		"\nstatic const std::vector<uint8_t> ${name} = { 0x00, };\n")
endforeach()

# Only touch the file when the content changes, so an unchanged embed
# does not relink every dependent target.
if(EXISTS ${OUT})
	file(READ ${OUT} current)
	string(COMPARE NOTEQUAL "${current}" "${HEADER_CONTENT}" changed)
	if(NOT changed)
		return()
	endif()
endif()
file(WRITE ${OUT} "${HEADER_CONTENT}")