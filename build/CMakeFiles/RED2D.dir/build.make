# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.10

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/corynull/Projects/red-c

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/corynull/Projects/red-c/build

# Include any dependencies generated for this target.
include CMakeFiles/RED2D.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/RED2D.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/RED2D.dir/flags.make

CMakeFiles/RED2D.dir/src/array.cpp.o: CMakeFiles/RED2D.dir/flags.make
CMakeFiles/RED2D.dir/src/array.cpp.o: ../src/array.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/corynull/Projects/red-c/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/RED2D.dir/src/array.cpp.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/RED2D.dir/src/array.cpp.o -c /home/corynull/Projects/red-c/src/array.cpp

CMakeFiles/RED2D.dir/src/array.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/RED2D.dir/src/array.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/corynull/Projects/red-c/src/array.cpp > CMakeFiles/RED2D.dir/src/array.cpp.i

CMakeFiles/RED2D.dir/src/array.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/RED2D.dir/src/array.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/corynull/Projects/red-c/src/array.cpp -o CMakeFiles/RED2D.dir/src/array.cpp.s

CMakeFiles/RED2D.dir/src/array.cpp.o.requires:

.PHONY : CMakeFiles/RED2D.dir/src/array.cpp.o.requires

CMakeFiles/RED2D.dir/src/array.cpp.o.provides: CMakeFiles/RED2D.dir/src/array.cpp.o.requires
	$(MAKE) -f CMakeFiles/RED2D.dir/build.make CMakeFiles/RED2D.dir/src/array.cpp.o.provides.build
.PHONY : CMakeFiles/RED2D.dir/src/array.cpp.o.provides

CMakeFiles/RED2D.dir/src/array.cpp.o.provides.build: CMakeFiles/RED2D.dir/src/array.cpp.o


CMakeFiles/RED2D.dir/src/component.cpp.o: CMakeFiles/RED2D.dir/flags.make
CMakeFiles/RED2D.dir/src/component.cpp.o: ../src/component.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/corynull/Projects/red-c/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object CMakeFiles/RED2D.dir/src/component.cpp.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/RED2D.dir/src/component.cpp.o -c /home/corynull/Projects/red-c/src/component.cpp

CMakeFiles/RED2D.dir/src/component.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/RED2D.dir/src/component.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/corynull/Projects/red-c/src/component.cpp > CMakeFiles/RED2D.dir/src/component.cpp.i

CMakeFiles/RED2D.dir/src/component.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/RED2D.dir/src/component.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/corynull/Projects/red-c/src/component.cpp -o CMakeFiles/RED2D.dir/src/component.cpp.s

CMakeFiles/RED2D.dir/src/component.cpp.o.requires:

.PHONY : CMakeFiles/RED2D.dir/src/component.cpp.o.requires

CMakeFiles/RED2D.dir/src/component.cpp.o.provides: CMakeFiles/RED2D.dir/src/component.cpp.o.requires
	$(MAKE) -f CMakeFiles/RED2D.dir/build.make CMakeFiles/RED2D.dir/src/component.cpp.o.provides.build
.PHONY : CMakeFiles/RED2D.dir/src/component.cpp.o.provides

CMakeFiles/RED2D.dir/src/component.cpp.o.provides.build: CMakeFiles/RED2D.dir/src/component.cpp.o


CMakeFiles/RED2D.dir/src/engine.cpp.o: CMakeFiles/RED2D.dir/flags.make
CMakeFiles/RED2D.dir/src/engine.cpp.o: ../src/engine.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/corynull/Projects/red-c/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building CXX object CMakeFiles/RED2D.dir/src/engine.cpp.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/RED2D.dir/src/engine.cpp.o -c /home/corynull/Projects/red-c/src/engine.cpp

CMakeFiles/RED2D.dir/src/engine.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/RED2D.dir/src/engine.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/corynull/Projects/red-c/src/engine.cpp > CMakeFiles/RED2D.dir/src/engine.cpp.i

CMakeFiles/RED2D.dir/src/engine.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/RED2D.dir/src/engine.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/corynull/Projects/red-c/src/engine.cpp -o CMakeFiles/RED2D.dir/src/engine.cpp.s

CMakeFiles/RED2D.dir/src/engine.cpp.o.requires:

.PHONY : CMakeFiles/RED2D.dir/src/engine.cpp.o.requires

CMakeFiles/RED2D.dir/src/engine.cpp.o.provides: CMakeFiles/RED2D.dir/src/engine.cpp.o.requires
	$(MAKE) -f CMakeFiles/RED2D.dir/build.make CMakeFiles/RED2D.dir/src/engine.cpp.o.provides.build
.PHONY : CMakeFiles/RED2D.dir/src/engine.cpp.o.provides

CMakeFiles/RED2D.dir/src/engine.cpp.o.provides.build: CMakeFiles/RED2D.dir/src/engine.cpp.o


CMakeFiles/RED2D.dir/src/entity.cpp.o: CMakeFiles/RED2D.dir/flags.make
CMakeFiles/RED2D.dir/src/entity.cpp.o: ../src/entity.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/corynull/Projects/red-c/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Building CXX object CMakeFiles/RED2D.dir/src/entity.cpp.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/RED2D.dir/src/entity.cpp.o -c /home/corynull/Projects/red-c/src/entity.cpp

CMakeFiles/RED2D.dir/src/entity.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/RED2D.dir/src/entity.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/corynull/Projects/red-c/src/entity.cpp > CMakeFiles/RED2D.dir/src/entity.cpp.i

CMakeFiles/RED2D.dir/src/entity.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/RED2D.dir/src/entity.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/corynull/Projects/red-c/src/entity.cpp -o CMakeFiles/RED2D.dir/src/entity.cpp.s

CMakeFiles/RED2D.dir/src/entity.cpp.o.requires:

.PHONY : CMakeFiles/RED2D.dir/src/entity.cpp.o.requires

CMakeFiles/RED2D.dir/src/entity.cpp.o.provides: CMakeFiles/RED2D.dir/src/entity.cpp.o.requires
	$(MAKE) -f CMakeFiles/RED2D.dir/build.make CMakeFiles/RED2D.dir/src/entity.cpp.o.provides.build
.PHONY : CMakeFiles/RED2D.dir/src/entity.cpp.o.provides

CMakeFiles/RED2D.dir/src/entity.cpp.o.provides.build: CMakeFiles/RED2D.dir/src/entity.cpp.o


CMakeFiles/RED2D.dir/src/graphics.cpp.o: CMakeFiles/RED2D.dir/flags.make
CMakeFiles/RED2D.dir/src/graphics.cpp.o: ../src/graphics.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/corynull/Projects/red-c/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Building CXX object CMakeFiles/RED2D.dir/src/graphics.cpp.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/RED2D.dir/src/graphics.cpp.o -c /home/corynull/Projects/red-c/src/graphics.cpp

CMakeFiles/RED2D.dir/src/graphics.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/RED2D.dir/src/graphics.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/corynull/Projects/red-c/src/graphics.cpp > CMakeFiles/RED2D.dir/src/graphics.cpp.i

CMakeFiles/RED2D.dir/src/graphics.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/RED2D.dir/src/graphics.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/corynull/Projects/red-c/src/graphics.cpp -o CMakeFiles/RED2D.dir/src/graphics.cpp.s

CMakeFiles/RED2D.dir/src/graphics.cpp.o.requires:

.PHONY : CMakeFiles/RED2D.dir/src/graphics.cpp.o.requires

CMakeFiles/RED2D.dir/src/graphics.cpp.o.provides: CMakeFiles/RED2D.dir/src/graphics.cpp.o.requires
	$(MAKE) -f CMakeFiles/RED2D.dir/build.make CMakeFiles/RED2D.dir/src/graphics.cpp.o.provides.build
.PHONY : CMakeFiles/RED2D.dir/src/graphics.cpp.o.provides

CMakeFiles/RED2D.dir/src/graphics.cpp.o.provides.build: CMakeFiles/RED2D.dir/src/graphics.cpp.o


CMakeFiles/RED2D.dir/src/main.cpp.o: CMakeFiles/RED2D.dir/flags.make
CMakeFiles/RED2D.dir/src/main.cpp.o: ../src/main.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/corynull/Projects/red-c/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_6) "Building CXX object CMakeFiles/RED2D.dir/src/main.cpp.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/RED2D.dir/src/main.cpp.o -c /home/corynull/Projects/red-c/src/main.cpp

CMakeFiles/RED2D.dir/src/main.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/RED2D.dir/src/main.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/corynull/Projects/red-c/src/main.cpp > CMakeFiles/RED2D.dir/src/main.cpp.i

CMakeFiles/RED2D.dir/src/main.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/RED2D.dir/src/main.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/corynull/Projects/red-c/src/main.cpp -o CMakeFiles/RED2D.dir/src/main.cpp.s

CMakeFiles/RED2D.dir/src/main.cpp.o.requires:

.PHONY : CMakeFiles/RED2D.dir/src/main.cpp.o.requires

CMakeFiles/RED2D.dir/src/main.cpp.o.provides: CMakeFiles/RED2D.dir/src/main.cpp.o.requires
	$(MAKE) -f CMakeFiles/RED2D.dir/build.make CMakeFiles/RED2D.dir/src/main.cpp.o.provides.build
.PHONY : CMakeFiles/RED2D.dir/src/main.cpp.o.provides

CMakeFiles/RED2D.dir/src/main.cpp.o.provides.build: CMakeFiles/RED2D.dir/src/main.cpp.o


CMakeFiles/RED2D.dir/src/mesh.cpp.o: CMakeFiles/RED2D.dir/flags.make
CMakeFiles/RED2D.dir/src/mesh.cpp.o: ../src/mesh.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/corynull/Projects/red-c/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_7) "Building CXX object CMakeFiles/RED2D.dir/src/mesh.cpp.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/RED2D.dir/src/mesh.cpp.o -c /home/corynull/Projects/red-c/src/mesh.cpp

CMakeFiles/RED2D.dir/src/mesh.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/RED2D.dir/src/mesh.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/corynull/Projects/red-c/src/mesh.cpp > CMakeFiles/RED2D.dir/src/mesh.cpp.i

CMakeFiles/RED2D.dir/src/mesh.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/RED2D.dir/src/mesh.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/corynull/Projects/red-c/src/mesh.cpp -o CMakeFiles/RED2D.dir/src/mesh.cpp.s

CMakeFiles/RED2D.dir/src/mesh.cpp.o.requires:

.PHONY : CMakeFiles/RED2D.dir/src/mesh.cpp.o.requires

CMakeFiles/RED2D.dir/src/mesh.cpp.o.provides: CMakeFiles/RED2D.dir/src/mesh.cpp.o.requires
	$(MAKE) -f CMakeFiles/RED2D.dir/build.make CMakeFiles/RED2D.dir/src/mesh.cpp.o.provides.build
.PHONY : CMakeFiles/RED2D.dir/src/mesh.cpp.o.provides

CMakeFiles/RED2D.dir/src/mesh.cpp.o.provides.build: CMakeFiles/RED2D.dir/src/mesh.cpp.o


CMakeFiles/RED2D.dir/src/object.cpp.o: CMakeFiles/RED2D.dir/flags.make
CMakeFiles/RED2D.dir/src/object.cpp.o: ../src/object.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/corynull/Projects/red-c/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_8) "Building CXX object CMakeFiles/RED2D.dir/src/object.cpp.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/RED2D.dir/src/object.cpp.o -c /home/corynull/Projects/red-c/src/object.cpp

CMakeFiles/RED2D.dir/src/object.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/RED2D.dir/src/object.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/corynull/Projects/red-c/src/object.cpp > CMakeFiles/RED2D.dir/src/object.cpp.i

CMakeFiles/RED2D.dir/src/object.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/RED2D.dir/src/object.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/corynull/Projects/red-c/src/object.cpp -o CMakeFiles/RED2D.dir/src/object.cpp.s

CMakeFiles/RED2D.dir/src/object.cpp.o.requires:

.PHONY : CMakeFiles/RED2D.dir/src/object.cpp.o.requires

CMakeFiles/RED2D.dir/src/object.cpp.o.provides: CMakeFiles/RED2D.dir/src/object.cpp.o.requires
	$(MAKE) -f CMakeFiles/RED2D.dir/build.make CMakeFiles/RED2D.dir/src/object.cpp.o.provides.build
.PHONY : CMakeFiles/RED2D.dir/src/object.cpp.o.provides

CMakeFiles/RED2D.dir/src/object.cpp.o.provides.build: CMakeFiles/RED2D.dir/src/object.cpp.o


CMakeFiles/RED2D.dir/src/renderable.cpp.o: CMakeFiles/RED2D.dir/flags.make
CMakeFiles/RED2D.dir/src/renderable.cpp.o: ../src/renderable.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/corynull/Projects/red-c/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_9) "Building CXX object CMakeFiles/RED2D.dir/src/renderable.cpp.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/RED2D.dir/src/renderable.cpp.o -c /home/corynull/Projects/red-c/src/renderable.cpp

CMakeFiles/RED2D.dir/src/renderable.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/RED2D.dir/src/renderable.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/corynull/Projects/red-c/src/renderable.cpp > CMakeFiles/RED2D.dir/src/renderable.cpp.i

CMakeFiles/RED2D.dir/src/renderable.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/RED2D.dir/src/renderable.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/corynull/Projects/red-c/src/renderable.cpp -o CMakeFiles/RED2D.dir/src/renderable.cpp.s

CMakeFiles/RED2D.dir/src/renderable.cpp.o.requires:

.PHONY : CMakeFiles/RED2D.dir/src/renderable.cpp.o.requires

CMakeFiles/RED2D.dir/src/renderable.cpp.o.provides: CMakeFiles/RED2D.dir/src/renderable.cpp.o.requires
	$(MAKE) -f CMakeFiles/RED2D.dir/build.make CMakeFiles/RED2D.dir/src/renderable.cpp.o.provides.build
.PHONY : CMakeFiles/RED2D.dir/src/renderable.cpp.o.provides

CMakeFiles/RED2D.dir/src/renderable.cpp.o.provides.build: CMakeFiles/RED2D.dir/src/renderable.cpp.o


CMakeFiles/RED2D.dir/src/sprite.cpp.o: CMakeFiles/RED2D.dir/flags.make
CMakeFiles/RED2D.dir/src/sprite.cpp.o: ../src/sprite.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/corynull/Projects/red-c/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_10) "Building CXX object CMakeFiles/RED2D.dir/src/sprite.cpp.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/RED2D.dir/src/sprite.cpp.o -c /home/corynull/Projects/red-c/src/sprite.cpp

CMakeFiles/RED2D.dir/src/sprite.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/RED2D.dir/src/sprite.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/corynull/Projects/red-c/src/sprite.cpp > CMakeFiles/RED2D.dir/src/sprite.cpp.i

CMakeFiles/RED2D.dir/src/sprite.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/RED2D.dir/src/sprite.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/corynull/Projects/red-c/src/sprite.cpp -o CMakeFiles/RED2D.dir/src/sprite.cpp.s

CMakeFiles/RED2D.dir/src/sprite.cpp.o.requires:

.PHONY : CMakeFiles/RED2D.dir/src/sprite.cpp.o.requires

CMakeFiles/RED2D.dir/src/sprite.cpp.o.provides: CMakeFiles/RED2D.dir/src/sprite.cpp.o.requires
	$(MAKE) -f CMakeFiles/RED2D.dir/build.make CMakeFiles/RED2D.dir/src/sprite.cpp.o.provides.build
.PHONY : CMakeFiles/RED2D.dir/src/sprite.cpp.o.provides

CMakeFiles/RED2D.dir/src/sprite.cpp.o.provides.build: CMakeFiles/RED2D.dir/src/sprite.cpp.o


CMakeFiles/RED2D.dir/src/types.cpp.o: CMakeFiles/RED2D.dir/flags.make
CMakeFiles/RED2D.dir/src/types.cpp.o: ../src/types.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/corynull/Projects/red-c/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_11) "Building CXX object CMakeFiles/RED2D.dir/src/types.cpp.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/RED2D.dir/src/types.cpp.o -c /home/corynull/Projects/red-c/src/types.cpp

CMakeFiles/RED2D.dir/src/types.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/RED2D.dir/src/types.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/corynull/Projects/red-c/src/types.cpp > CMakeFiles/RED2D.dir/src/types.cpp.i

CMakeFiles/RED2D.dir/src/types.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/RED2D.dir/src/types.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/corynull/Projects/red-c/src/types.cpp -o CMakeFiles/RED2D.dir/src/types.cpp.s

CMakeFiles/RED2D.dir/src/types.cpp.o.requires:

.PHONY : CMakeFiles/RED2D.dir/src/types.cpp.o.requires

CMakeFiles/RED2D.dir/src/types.cpp.o.provides: CMakeFiles/RED2D.dir/src/types.cpp.o.requires
	$(MAKE) -f CMakeFiles/RED2D.dir/build.make CMakeFiles/RED2D.dir/src/types.cpp.o.provides.build
.PHONY : CMakeFiles/RED2D.dir/src/types.cpp.o.provides

CMakeFiles/RED2D.dir/src/types.cpp.o.provides.build: CMakeFiles/RED2D.dir/src/types.cpp.o


CMakeFiles/RED2D.dir/src/var.cpp.o: CMakeFiles/RED2D.dir/flags.make
CMakeFiles/RED2D.dir/src/var.cpp.o: ../src/var.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/corynull/Projects/red-c/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_12) "Building CXX object CMakeFiles/RED2D.dir/src/var.cpp.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/RED2D.dir/src/var.cpp.o -c /home/corynull/Projects/red-c/src/var.cpp

CMakeFiles/RED2D.dir/src/var.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/RED2D.dir/src/var.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/corynull/Projects/red-c/src/var.cpp > CMakeFiles/RED2D.dir/src/var.cpp.i

CMakeFiles/RED2D.dir/src/var.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/RED2D.dir/src/var.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/corynull/Projects/red-c/src/var.cpp -o CMakeFiles/RED2D.dir/src/var.cpp.s

CMakeFiles/RED2D.dir/src/var.cpp.o.requires:

.PHONY : CMakeFiles/RED2D.dir/src/var.cpp.o.requires

CMakeFiles/RED2D.dir/src/var.cpp.o.provides: CMakeFiles/RED2D.dir/src/var.cpp.o.requires
	$(MAKE) -f CMakeFiles/RED2D.dir/build.make CMakeFiles/RED2D.dir/src/var.cpp.o.provides.build
.PHONY : CMakeFiles/RED2D.dir/src/var.cpp.o.provides

CMakeFiles/RED2D.dir/src/var.cpp.o.provides.build: CMakeFiles/RED2D.dir/src/var.cpp.o


CMakeFiles/RED2D.dir/src/window.cpp.o: CMakeFiles/RED2D.dir/flags.make
CMakeFiles/RED2D.dir/src/window.cpp.o: ../src/window.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/corynull/Projects/red-c/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_13) "Building CXX object CMakeFiles/RED2D.dir/src/window.cpp.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/RED2D.dir/src/window.cpp.o -c /home/corynull/Projects/red-c/src/window.cpp

CMakeFiles/RED2D.dir/src/window.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/RED2D.dir/src/window.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/corynull/Projects/red-c/src/window.cpp > CMakeFiles/RED2D.dir/src/window.cpp.i

CMakeFiles/RED2D.dir/src/window.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/RED2D.dir/src/window.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/corynull/Projects/red-c/src/window.cpp -o CMakeFiles/RED2D.dir/src/window.cpp.s

CMakeFiles/RED2D.dir/src/window.cpp.o.requires:

.PHONY : CMakeFiles/RED2D.dir/src/window.cpp.o.requires

CMakeFiles/RED2D.dir/src/window.cpp.o.provides: CMakeFiles/RED2D.dir/src/window.cpp.o.requires
	$(MAKE) -f CMakeFiles/RED2D.dir/build.make CMakeFiles/RED2D.dir/src/window.cpp.o.provides.build
.PHONY : CMakeFiles/RED2D.dir/src/window.cpp.o.provides

CMakeFiles/RED2D.dir/src/window.cpp.o.provides.build: CMakeFiles/RED2D.dir/src/window.cpp.o


# Object files for target RED2D
RED2D_OBJECTS = \
"CMakeFiles/RED2D.dir/src/array.cpp.o" \
"CMakeFiles/RED2D.dir/src/component.cpp.o" \
"CMakeFiles/RED2D.dir/src/engine.cpp.o" \
"CMakeFiles/RED2D.dir/src/entity.cpp.o" \
"CMakeFiles/RED2D.dir/src/graphics.cpp.o" \
"CMakeFiles/RED2D.dir/src/main.cpp.o" \
"CMakeFiles/RED2D.dir/src/mesh.cpp.o" \
"CMakeFiles/RED2D.dir/src/object.cpp.o" \
"CMakeFiles/RED2D.dir/src/renderable.cpp.o" \
"CMakeFiles/RED2D.dir/src/sprite.cpp.o" \
"CMakeFiles/RED2D.dir/src/types.cpp.o" \
"CMakeFiles/RED2D.dir/src/var.cpp.o" \
"CMakeFiles/RED2D.dir/src/window.cpp.o"

# External object files for target RED2D
RED2D_EXTERNAL_OBJECTS =

RED2D: CMakeFiles/RED2D.dir/src/array.cpp.o
RED2D: CMakeFiles/RED2D.dir/src/component.cpp.o
RED2D: CMakeFiles/RED2D.dir/src/engine.cpp.o
RED2D: CMakeFiles/RED2D.dir/src/entity.cpp.o
RED2D: CMakeFiles/RED2D.dir/src/graphics.cpp.o
RED2D: CMakeFiles/RED2D.dir/src/main.cpp.o
RED2D: CMakeFiles/RED2D.dir/src/mesh.cpp.o
RED2D: CMakeFiles/RED2D.dir/src/object.cpp.o
RED2D: CMakeFiles/RED2D.dir/src/renderable.cpp.o
RED2D: CMakeFiles/RED2D.dir/src/sprite.cpp.o
RED2D: CMakeFiles/RED2D.dir/src/types.cpp.o
RED2D: CMakeFiles/RED2D.dir/src/var.cpp.o
RED2D: CMakeFiles/RED2D.dir/src/window.cpp.o
RED2D: CMakeFiles/RED2D.dir/build.make
RED2D: CMakeFiles/RED2D.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/corynull/Projects/red-c/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_14) "Linking CXX executable RED2D"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/RED2D.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/RED2D.dir/build: RED2D

.PHONY : CMakeFiles/RED2D.dir/build

CMakeFiles/RED2D.dir/requires: CMakeFiles/RED2D.dir/src/array.cpp.o.requires
CMakeFiles/RED2D.dir/requires: CMakeFiles/RED2D.dir/src/component.cpp.o.requires
CMakeFiles/RED2D.dir/requires: CMakeFiles/RED2D.dir/src/engine.cpp.o.requires
CMakeFiles/RED2D.dir/requires: CMakeFiles/RED2D.dir/src/entity.cpp.o.requires
CMakeFiles/RED2D.dir/requires: CMakeFiles/RED2D.dir/src/graphics.cpp.o.requires
CMakeFiles/RED2D.dir/requires: CMakeFiles/RED2D.dir/src/main.cpp.o.requires
CMakeFiles/RED2D.dir/requires: CMakeFiles/RED2D.dir/src/mesh.cpp.o.requires
CMakeFiles/RED2D.dir/requires: CMakeFiles/RED2D.dir/src/object.cpp.o.requires
CMakeFiles/RED2D.dir/requires: CMakeFiles/RED2D.dir/src/renderable.cpp.o.requires
CMakeFiles/RED2D.dir/requires: CMakeFiles/RED2D.dir/src/sprite.cpp.o.requires
CMakeFiles/RED2D.dir/requires: CMakeFiles/RED2D.dir/src/types.cpp.o.requires
CMakeFiles/RED2D.dir/requires: CMakeFiles/RED2D.dir/src/var.cpp.o.requires
CMakeFiles/RED2D.dir/requires: CMakeFiles/RED2D.dir/src/window.cpp.o.requires

.PHONY : CMakeFiles/RED2D.dir/requires

CMakeFiles/RED2D.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/RED2D.dir/cmake_clean.cmake
.PHONY : CMakeFiles/RED2D.dir/clean

CMakeFiles/RED2D.dir/depend:
	cd /home/corynull/Projects/red-c/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/corynull/Projects/red-c /home/corynull/Projects/red-c /home/corynull/Projects/red-c/build /home/corynull/Projects/red-c/build /home/corynull/Projects/red-c/build/CMakeFiles/RED2D.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/RED2D.dir/depend

