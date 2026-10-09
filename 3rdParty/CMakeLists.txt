cmake_minimum_required(VERSION 3.16...4.2)

project(gold)

cmake_policy(SET CMP0079 NEW)

if(NOT MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-gnu-zero-variadic-macro-arguments")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-gnu-zero-variadic-macro-arguments")
endif(NOT MSVC)

# Bullet is NOT built here: physics runs through the loadable
# libgoldBullet plugin compiled against the system bullet package (see
# cmake/game.cmake). Vendored physics sources were removed.

# FreeType is NOT built here: the UI module opens the system FreeType at
# runtime through the plugin loader (src/ui/ftShim*), falling back to the
# built-in 5x7 font when it is unavailable.
