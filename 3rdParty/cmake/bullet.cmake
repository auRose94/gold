cmake_minimum_required(VERSION 3.16)
set(BULLET_PHYSICS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bullet3/src)
set(LinearMath_SRC_DIR ${BULLET_PHYSICS_SOURCE_DIR}/LinearMath)
set(BulletSoftBody_SRC_DIR ${BULLET_PHYSICS_SOURCE_DIR}/BulletSoftBody)
set(BulletInverseDynamics_SRC_DIR ${BULLET_PHYSICS_SOURCE_DIR}/BulletInverseDynamics)
set(BulletDynamics_SRC_DIR ${BULLET_PHYSICS_SOURCE_DIR}/BulletDynamics)
set(BulletCollision_SRC_DIR ${BULLET_PHYSICS_SOURCE_DIR}/BulletCollision)
set(Bullet2FileLoader_SRC_DIR ${BULLET_PHYSICS_SOURCE_DIR}/Bullet3Serialize/Bullet2FileLoader)
set(Bullet3OpenCL_clew_SRC_DIR ${BULLET_PHYSICS_SOURCE_DIR}/Bullet3OpenCL)
set(Bullet3Geometry_SRC_DIR ${BULLET_PHYSICS_SOURCE_DIR}/Bullet3Geometry)
set(Bullet3Dynamics_SRC_DIR ${BULLET_PHYSICS_SOURCE_DIR}/Bullet3Dynamics)
set(Bullet3Common_SRC_DIR ${BULLET_PHYSICS_SOURCE_DIR}/Bullet3Common)
set(Bullet3Collision_SRC_DIR ${BULLET_PHYSICS_SOURCE_DIR}/Bullet3Collision)
set(InverseDynamics_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bullet3/Extras/InverseDynamics)
message("Bullet SRC DIR: ${BULLET_PHYSICS_SOURCE_DIR}")

cmake_minimum_required(VERSION 3.16)
set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS true)
cmake_policy(SET CMP0017 NEW)
#this line has to appear before 'PROJECT' in order to be able to disable incremental linking
SET(MSVC_INCREMENTAL_DEFAULT ON)

PROJECT(BULLET_PHYSICS)
FILE (STRINGS bullet3/VERSION BULLET_VERSION)

IF(COMMAND cmake_policy)
   cmake_policy(SET CMP0003 NEW)
   if(POLICY CMP0042)
      # Enable MACOSX_RPATH by default.
      cmake_policy(SET CMP0042 NEW)
   endif(POLICY CMP0042)
ENDIF(COMMAND cmake_policy)

IF (NOT CMAKE_BUILD_TYPE)
# SET(CMAKE_BUILD_TYPE "Debug")
 SET(CMAKE_BUILD_TYPE "Release")
ENDIF (NOT CMAKE_BUILD_TYPE)

SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")
#MESSAGE("CMAKE_CXX_FLAGS_DEBUG="+${CMAKE_CXX_FLAGS_DEBUG})

OPTION(USE_DOUBLE_PRECISION "Use double precision"	ON)
SET(CLAMP_VELOCITIES "0" CACHE STRING "Clamp rigid bodies' velocity to this value, if larger than zero. Useful to prevent floating point errors or in general runaway velocities in complex scenarios")
OPTION(USE_SOFT_BODY_MULTI_BODY_DYNAMICS_WORLD "Use btSoftMultiBodyDynamicsWorld" ON)

OPTION(BULLET2_MULTITHREADING "Build Bullet 2 libraries with mutex locking around certain operations (required for multi-threading)" OFF)
IF (BULLET2_MULTITHREADING)
    OPTION(BULLET2_USE_OPEN_MP_MULTITHREADING "Build Bullet 2 with support for multi-threading with OpenMP (requires a compiler with OpenMP support)" OFF)
    OPTION(BULLET2_USE_TBB_MULTITHREADING "Build Bullet 2 with support for multi-threading with Intel Threading Building Blocks (requires the TBB library to be already installed)" OFF)
    IF (MSVC)
        OPTION(BULLET2_USE_PPL_MULTITHREADING "Build Bullet 2 with support for multi-threading with Microsoft Parallel Patterns Library (requires MSVC compiler)" OFF)
    ENDIF (MSVC)
ENDIF (BULLET2_MULTITHREADING)


IF(NOT WIN32)
	SET(DL ${CMAKE_DL_LIBS})
	IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
		MESSAGE("Linux")
		SET(OSDEF -D_LINUX)
	ELSE(CMAKE_SYSTEM_NAME MATCHES "Linux")
		IF(APPLE)
			MESSAGE("Apple")
			SET(OSDEF -D_DARWIN)
		ELSE(APPLE)
			MESSAGE("BSD?")
			SET(OSDEF -D_BSD)
		ENDIF(APPLE)
	ENDIF(CMAKE_SYSTEM_NAME MATCHES "Linux")
ENDIF(NOT WIN32)

OPTION(USE_MSVC_INCREMENTAL_LINKING "Use MSVC Incremental Linking" OFF)

#statically linking VC++ isn't supported for WindowsPhone/WindowsStore
IF (CMAKE_SYSTEM_NAME STREQUAL WindowsPhone OR CMAKE_SYSTEM_NAME STREQUAL WindowsStore)
	OPTION(USE_MSVC_RUNTIME_LIBRARY_DLL "Use MSVC Runtime Library DLL (/MD or /MDd)" ON)
ELSE ()
	OPTION(USE_MSVC_RUNTIME_LIBRARY_DLL "Use MSVC Runtime Library DLL (/MD or /MDd)" OFF)
ENDIF (CMAKE_SYSTEM_NAME STREQUAL WindowsPhone OR CMAKE_SYSTEM_NAME STREQUAL WindowsStore)
OPTION(USE_MSVC_RELEASE_RUNTIME_ALWAYS "Use MSVC Release Runtime Library even in Debug" OFF)

#SET(CMAKE_EXE_LINKER_FLAGS_INIT    "/STACK:10000000 /INCREMENTAL:NO")
#SET(CMAKE_EXE_LINKER_FLAGS    "/STACK:10000000 /INCREMENTAL:NO")

#MESSAGE("MSVC_INCREMENTAL_YES_FLAG"+${MSVC_INCREMENTAL_YES_FLAG})


IF(MSVC)
	IF (NOT USE_MSVC_INCREMENTAL_LINKING)
		#MESSAGE("MSVC_INCREMENTAL_DEFAULT"+${MSVC_INCREMENTAL_DEFAULT})
		SET( MSVC_INCREMENTAL_YES_FLAG "/INCREMENTAL:NO")

		STRING(REPLACE "INCREMENTAL:YES" "INCREMENTAL:NO" replacementFlags ${CMAKE_EXE_LINKER_FLAGS_DEBUG})
		SET(CMAKE_EXE_LINKER_FLAGS_DEBUG "/INCREMENTAL:NO ${replacementFlags}" )
		MESSAGE("CMAKE_EXE_LINKER_FLAGS_DEBUG=${CMAKE_EXE_LINKER_FLAGS_DEBUG}")

		STRING(REPLACE "INCREMENTAL:YES" "INCREMENTAL:NO" replacementFlags2 ${CMAKE_EXE_LINKER_FLAGS})

		SET(CMAKE_EXE_LINKER_FLAGS ${replacementFlag2})
		STRING(REPLACE "INCREMENTAL:YES" "" replacementFlags3 "${CMAKE_EXTRA_LINK_FLAGS}")

		SET(CMAKE_EXTRA_LINK_FLAGS ${replacementFlag3})


		STRING(REPLACE "INCREMENTAL:YES" "INCREMENTAL:NO" replacementFlags3 "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO}")
		SET(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO ${replacementFlags3})
		SET(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "/INCREMENTAL:NO ${replacementFlags3}" )

	ENDIF (NOT USE_MSVC_INCREMENTAL_LINKING)

	IF (NOT USE_MSVC_RUNTIME_LIBRARY_DLL)
		#We statically link to reduce dependencies
		FOREACH(flag_var CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO )
			IF(${flag_var} MATCHES "/MD")
				STRING(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
			ENDIF(${flag_var} MATCHES "/MD")
			IF(${flag_var} MATCHES "/MDd")
				STRING(REGEX REPLACE "/MDd" "/MTd" ${flag_var} "${${flag_var}}")
			ENDIF(${flag_var} MATCHES "/MDd")
		ENDFOREACH(flag_var)
	ENDIF (NOT USE_MSVC_RUNTIME_LIBRARY_DLL)

	IF (USE_MSVC_RELEASE_RUNTIME_ALWAYS)
		FOREACH(flag_var CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO )
			IF(${flag_var} MATCHES "/MDd")
				STRING(REGEX REPLACE "/MDd" "/MD" ${flag_var} "${${flag_var}}")
			ENDIF(${flag_var} MATCHES "/MDd")
			IF(${flag_var} MATCHES "/MTd")
				STRING(REGEX REPLACE "/MTd" "/MT" ${flag_var} "${${flag_var}}")
			ENDIF(${flag_var} MATCHES "/MTd")
			# Need to remove _DEBUG too otherwise things like _ITERATOR_DEBUG_LEVEL mismatch
			IF(${flag_var} MATCHES "-D_DEBUG")
				STRING(REGEX REPLACE "-D_DEBUG" "" ${flag_var} "${${flag_var}}")
			ENDIF(${flag_var} MATCHES "-D_DEBUG")
		ENDFOREACH(flag_var)
	ENDIF (USE_MSVC_RELEASE_RUNTIME_ALWAYS)

	IF (CMAKE_CL_64)
	  ADD_DEFINITIONS(-D_WIN64)
	ELSE()
	  OPTION(USE_MSVC_SSE "Use MSVC /arch:sse option"	OFF)
	  option(USE_MSVC_SSE2 "Compile your program with SSE2 instructions" ON)

	  IF (USE_MSVC_SSE)
		  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:SSE")
	  ENDIF()
	  IF (USE_MSVC_SSE2)
		  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:SSE2")
	  ENDIF()

	ENDIF()

	option(USE_MSVC_AVX "Compile your program with AVX instructions"  OFF)

	IF(USE_MSVC_AVX)
		add_definitions(/arch:AVX)
	ENDIF()

	OPTION(USE_MSVC_FAST_FLOATINGPOINT "Use MSVC /fp:fast option"	ON)
	IF (USE_MSVC_FAST_FLOATINGPOINT)
		  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:fast")
  ENDIF()

	OPTION(USE_MSVC_STRING_POOLING "Use MSVC /GF string pooling option"	ON)
	IF (USE_MSVC_STRING_POOLING)
		SET(CMAKE_C_FLAGS "/GF ${CMAKE_C_FLAGS}")
		SET(CMAKE_CXX_FLAGS "/GF ${CMAKE_CXX_FLAGS}")
	ENDIF()

	OPTION(USE_MSVC_FUNCTION_LEVEL_LINKING "Use MSVC /Gy function level linking option"	ON)
	IF(USE_MSVC_FUNCTION_LEVEL_LINKING)
		SET(CMAKE_C_FLAGS "/Gy ${CMAKE_C_FLAGS}")
		SET(CMAKE_CXX_FLAGS "/Gy ${CMAKE_CXX_FLAGS}")
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /OPT:REF")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /OPT:REF")
	ENDIF(USE_MSVC_FUNCTION_LEVEL_LINKING)

	OPTION(USE_MSVC_EXEPTIONS "Use MSVC C++ exceptions option"	OFF)



	OPTION(USE_MSVC_COMDAT_FOLDING "Use MSVC /OPT:ICF COMDAT folding option"	ON)

	IF(USE_MSVC_COMDAT_FOLDING)
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /OPT:ICF")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /OPT:ICF")
	ENDIF()

	OPTION(USE_MSVC_DISABLE_RTTI "Use MSVC /GR- disabled RTTI flags option"	ON)
	IF(USE_MSVC_DISABLE_RTTI)
	  STRING(REGEX REPLACE "/GR" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS}) # Disable RTTI
		SET(CMAKE_C_FLAGS "/GR- ${CMAKE_C_FLAGS}")
		SET(CMAKE_CXX_FLAGS "/GR- ${CMAKE_CXX_FLAGS}")
	ENDIF(USE_MSVC_DISABLE_RTTI)

	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4244 /wd4267")
ENDIF(MSVC)



IF (WIN32)
OPTION(INTERNAL_CREATE_DISTRIBUTABLE_MSVC_PROJECTFILES "Create MSVC projectfiles that can be distributed" OFF)


OPTION(INTERNAL_CREATE_MSVC_RELATIVE_PATH_PROJECTFILES "Create MSVC projectfiles with relative paths" OFF)
OPTION(INTERNAL_ADD_POSTFIX_EXECUTABLE_NAMES "Add MSVC postfix for executable names (_Debug)" OFF)

SET(CMAKE_DEBUG_POSTFIX "_Debug" CACHE STRING "Adds a postfix for debug-built libraries.")
SET(CMAKE_MINSIZEREL_POSTFIX "_MinsizeRel" CACHE STRING "Adds a postfix for MinsizeRelease-built libraries.")
SET(CMAKE_RELWITHDEBINFO_POSTFIX "_RelWithDebugInfo" CACHE STRING "Adds a postfix for ReleaseWithDebug-built libraries.")





IF (INTERNAL_CREATE_MSVC_RELATIVE_PATH_PROJECTFILES)
SET(CMAKE_SUPPRESS_REGENERATION  1)
SET(CMAKE_USE_RELATIVE_PATHS 1)
ENDIF(INTERNAL_CREATE_MSVC_RELATIVE_PATH_PROJECTFILES)

ENDIF (WIN32)




OPTION(INTERNAL_UPDATE_SERIALIZATION_STRUCTURES "Internal update serialization structures" OFF)
IF (INTERNAL_UPDATE_SERIALIZATION_STRUCTURES)
ADD_DEFINITIONS( -DBT_INTERNAL_UPDATE_SERIALIZATION_STRUCTURES)
ENDIF (INTERNAL_UPDATE_SERIALIZATION_STRUCTURES)

IF (CLAMP_VELOCITIES)
ADD_DEFINITIONS( -DBT_CLAMP_VELOCITY_TO=${CLAMP_VELOCITIES})
ENDIF (CLAMP_VELOCITIES)

IF (USE_DOUBLE_PRECISION)
ADD_DEFINITIONS( -DBT_USE_DOUBLE_PRECISION)
SET( BULLET_DOUBLE_DEF "-DBT_USE_DOUBLE_PRECISION")
ENDIF (USE_DOUBLE_PRECISION)

IF (NOT USE_SOFT_BODY_MULTI_BODY_DYNAMICS_WORLD)
ADD_DEFINITIONS(-DSKIP_SOFT_BODY_MULTI_BODY_DYNAMICS_WORLD)
ENDIF ()

IF(USE_GRAPHICAL_BENCHMARK)
ADD_DEFINITIONS( -DUSE_GRAPHICAL_BENCHMARK)
ENDIF (USE_GRAPHICAL_BENCHMARK)

IF(BULLET2_MULTITHREADING)
	ADD_DEFINITIONS( -DBT_THREADSAFE=1 )
	IF (NOT MSVC)
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
	ENDIF (NOT MSVC)
	IF (NOT WIN32)
		LINK_LIBRARIES( pthread )
	ENDIF (NOT WIN32)
ENDIF (BULLET2_MULTITHREADING)

IF (BULLET2_USE_OPEN_MP_MULTITHREADING)
    ADD_DEFINITIONS("-DBT_USE_OPENMP=1")
    IF (MSVC)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /openmp")
    ELSE (MSVC)
        # GCC, Clang
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp")
    ENDIF (MSVC)
ENDIF (BULLET2_USE_OPEN_MP_MULTITHREADING)

IF (BULLET2_USE_TBB_MULTITHREADING)
    SET (BULLET2_TBB_INCLUDE_DIR "not found" CACHE PATH "Directory for Intel TBB includes.")
    SET (BULLET2_TBB_LIB_DIR "not found" CACHE PATH "Directory for Intel TBB libraries.")
    find_library(TBB_LIBRARY tbb PATHS ${BULLET2_TBB_LIB_DIR})
    find_library(TBBMALLOC_LIBRARY tbbmalloc PATHS ${BULLET2_TBB_LIB_DIR})
    ADD_DEFINITIONS("-DBT_USE_TBB=1")
    INCLUDE_DIRECTORIES( ${BULLET2_TBB_INCLUDE_DIR} )
    LINK_LIBRARIES( ${TBB_LIBRARY} ${TBBMALLOC_LIBRARY} )
ENDIF (BULLET2_USE_TBB_MULTITHREADING)

IF (BULLET2_USE_PPL_MULTITHREADING)
    ADD_DEFINITIONS("-DBT_USE_PPL=1")
ENDIF (BULLET2_USE_PPL_MULTITHREADING)

IF (WIN32)
ADD_DEFINITIONS( -D_CRT_SECURE_NO_WARNINGS )
ADD_DEFINITIONS( -D_CRT_SECURE_NO_DEPRECATE )
ADD_DEFINITIONS( -D_SCL_SECURE_NO_WARNINGS )
ENDIF(WIN32)


IF(COMMAND cmake_policy)
   cmake_policy(SET CMP0003 NEW)
ENDIF(COMMAND cmake_policy)


# This is the shortcut to finding GLU, GLUT and OpenGL if they are properly installed on your system
# This should be the case.

FIND_PACKAGE(OpenGL)
IF (OPENGL_FOUND)
	MESSAGE("OPENGL FOUND")
	MESSAGE(${OPENGL_LIBRARIES})
ELSE (OPENGL_FOUND)
	MESSAGE("OPENGL NOT FOUND")
	SET(OPENGL_gl_LIBRARY opengl32)
	SET(OPENGL_glu_LIBRARY glu32)
ENDIF (OPENGL_FOUND)


#FIND_PACKAGE(GLU)


IF (APPLE)
  FIND_LIBRARY(COCOA_LIBRARY Cocoa)
ENDIF()

set (BULLET_CONFIG_CMAKE_PATH lib${LIB_SUFFIX}/cmake/bullet )
list (APPEND BULLET_DEFINITIONS ${BULLET_DOUBLE_DEF})
list (APPEND BULLET_LIBRARIES LinearMath)
list (APPEND BULLET_LIBRARIES Bullet3Common)
list (APPEND BULLET_LIBRARIES BulletInverseDynamics)
list (APPEND BULLET_LIBRARIES BulletCollision)
list (APPEND BULLET_LIBRARIES BulletDynamics)
list (APPEND BULLET_LIBRARIES BulletSoftBody)
set (BULLET_USE_FILE ${BULLET_CONFIG_CMAKE_PATH}/UseBullet.cmake)
configure_file 	( ${CMAKE_CURRENT_SOURCE_DIR}/bullet3/BulletConfig.cmake.in
					${CMAKE_CURRENT_BINARY_DIR}/BulletConfig.cmake
					@ONLY ESCAPE_QUOTES
				)

#LinearMath

SET(LinearMath_SRCS
	${LinearMath_SRC_DIR}/btAlignedAllocator.cpp
	${LinearMath_SRC_DIR}/btConvexHull.cpp
	${LinearMath_SRC_DIR}/btConvexHullComputer.cpp
	${LinearMath_SRC_DIR}/btGeometryUtil.cpp
	${LinearMath_SRC_DIR}/btPolarDecomposition.cpp
	${LinearMath_SRC_DIR}/btQuickprof.cpp
	${LinearMath_SRC_DIR}/btReducedVector.cpp
	${LinearMath_SRC_DIR}/btSerializer.cpp
	${LinearMath_SRC_DIR}/btSerializer64.cpp
	${LinearMath_SRC_DIR}/btThreads.cpp
	${LinearMath_SRC_DIR}/btVector3.cpp
	${LinearMath_SRC_DIR}/TaskScheduler/btTaskScheduler.cpp
	${LinearMath_SRC_DIR}/TaskScheduler/btThreadSupportPosix.cpp
	${LinearMath_SRC_DIR}/TaskScheduler/btThreadSupportWin32.cpp
)

SET(LinearMath_HDRS
	${LinearMath_SRC_DIR}/btAabbUtil2.h
	${LinearMath_SRC_DIR}/btAlignedAllocator.h
	${LinearMath_SRC_DIR}/btAlignedObjectArray.h
	${LinearMath_SRC_DIR}/btConvexHull.h
	${LinearMath_SRC_DIR}/btConvexHullComputer.h
	${LinearMath_SRC_DIR}/btDefaultMotionState.h
	${LinearMath_SRC_DIR}/btGeometryUtil.h
	${LinearMath_SRC_DIR}/btGrahamScan2dConvexHull.h
	${LinearMath_SRC_DIR}/btHashMap.h
	${LinearMath_SRC_DIR}/btIDebugDraw.h
	${LinearMath_SRC_DIR}/btList.h
	${LinearMath_SRC_DIR}/btMatrix3x3.h
	${LinearMath_SRC_DIR}/btImplicitQRSVD.h
	${LinearMath_SRC_DIR}/btMinMax.h
	${LinearMath_SRC_DIR}/btModifiedGramSchmidt.h
	${LinearMath_SRC_DIR}/btMotionState.h
	${LinearMath_SRC_DIR}/btPolarDecomposition.h
	${LinearMath_SRC_DIR}/btPoolAllocator.h
	${LinearMath_SRC_DIR}/btQuadWord.h
	${LinearMath_SRC_DIR}/btQuaternion.h
	${LinearMath_SRC_DIR}/btQuickprof.h
	${LinearMath_SRC_DIR}/btReducedVector.h
	${LinearMath_SRC_DIR}/btRandom.h
	${LinearMath_SRC_DIR}/btScalar.h
	${LinearMath_SRC_DIR}/btSerializer.h
	${LinearMath_SRC_DIR}/btStackAlloc.h
	${LinearMath_SRC_DIR}/btThreads.h
	${LinearMath_SRC_DIR}/btTransform.h
	${LinearMath_SRC_DIR}/btTransformUtil.h
	${LinearMath_SRC_DIR}/btVector3.h
	${LinearMath_SRC_DIR}/TaskScheduler/btThreadSupportInterface.h
)

ADD_LIBRARY(LinearMath STATIC ${LinearMath_SRCS} ${LinearMath_HDRS})
target_include_directories(LinearMath 
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/bullet3/src
	PRIVATE
		${LinearMath_SRC_DIR}
	PUBLIC 
		${LinearMath_SRC_DIR}
		$<BUILD_INTERFACE:${LinearMath_SRC_DIR}>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(LinearMath PROPERTIES SOURCE_DIR ${LinearMath_SRC_DIR})
SET_TARGET_PROPERTIES(LinearMath PROPERTIES VERSION ${BULLET_VERSION})
SET_TARGET_PROPERTIES(LinearMath PROPERTIES SOVERSION ${BULLET_VERSION})

#BulletSoftBody

SET(BulletSoftBody_SRCS
	${BulletSoftBody_SRC_DIR}/btSoftBody.cpp
	${BulletSoftBody_SRC_DIR}/btSoftBodyConcaveCollisionAlgorithm.cpp
	${BulletSoftBody_SRC_DIR}/btSoftBodyHelpers.cpp
	${BulletSoftBody_SRC_DIR}/btSoftBodyRigidBodyCollisionConfiguration.cpp
	${BulletSoftBody_SRC_DIR}/btSoftRigidCollisionAlgorithm.cpp
	${BulletSoftBody_SRC_DIR}/btSoftRigidDynamicsWorld.cpp
	${BulletSoftBody_SRC_DIR}/btSoftMultiBodyDynamicsWorld.cpp
	${BulletSoftBody_SRC_DIR}/btSoftSoftCollisionAlgorithm.cpp
	${BulletSoftBody_SRC_DIR}/btDefaultSoftBodySolver.cpp

	${BulletSoftBody_SRC_DIR}/btDeformableBackwardEulerObjective.cpp
	${BulletSoftBody_SRC_DIR}/btDeformableBodySolver.cpp
	${BulletSoftBody_SRC_DIR}/btDeformableMultiBodyConstraintSolver.cpp
	${BulletSoftBody_SRC_DIR}/btDeformableContactProjection.cpp
	${BulletSoftBody_SRC_DIR}/btDeformableMultiBodyDynamicsWorld.cpp
	${BulletSoftBody_SRC_DIR}/btDeformableContactConstraint.cpp
	${BulletSoftBody_SRC_DIR}/poly34.cpp
)


SET(BulletSoftBody_HDRS
	${BulletSoftBody_SRC_DIR}/btSoftBody.h
	${BulletSoftBody_SRC_DIR}/btSoftBodyData.h
	${BulletSoftBody_SRC_DIR}/btSoftBodyConcaveCollisionAlgorithm.h
	${BulletSoftBody_SRC_DIR}/btSoftBodyHelpers.h
	${BulletSoftBody_SRC_DIR}/btSoftBodyRigidBodyCollisionConfiguration.h
	${BulletSoftBody_SRC_DIR}/btSoftRigidCollisionAlgorithm.h
	${BulletSoftBody_SRC_DIR}/btSoftRigidDynamicsWorld.h
	${BulletSoftBody_SRC_DIR}/btSoftMultiBodyDynamicsWorld.h
	${BulletSoftBody_SRC_DIR}/btSoftSoftCollisionAlgorithm.h
	${BulletSoftBody_SRC_DIR}/btSparseSDF.h

	${BulletSoftBody_SRC_DIR}/btSoftBodySolvers.h
	${BulletSoftBody_SRC_DIR}/btDefaultSoftBodySolver.h
	
	${BulletSoftBody_SRC_DIR}/btCGProjection.h
	${BulletSoftBody_SRC_DIR}/btConjugateGradient.h
	${BulletSoftBody_SRC_DIR}/btConjugateResidual.h
	${BulletSoftBody_SRC_DIR}/btDeformableGravityForce.h
	${BulletSoftBody_SRC_DIR}/btDeformableMassSpringForce.h
	${BulletSoftBody_SRC_DIR}/btDeformableCorotatedForce.h
	${BulletSoftBody_SRC_DIR}/btDeformableNeoHookeanForce.h
	${BulletSoftBody_SRC_DIR}/btDeformableLinearElasticityForce.h
	${BulletSoftBody_SRC_DIR}/btDeformableLagrangianForce.h
	${BulletSoftBody_SRC_DIR}/btPreconditioner.h

	${BulletSoftBody_SRC_DIR}/btDeformableBackwardEulerObjective.h
	${BulletSoftBody_SRC_DIR}/btDeformableBodySolver.h
	${BulletSoftBody_SRC_DIR}/btDeformableMultiBodyConstraintSolver.h
	${BulletSoftBody_SRC_DIR}/btDeformableContactProjection.h
	${BulletSoftBody_SRC_DIR}/btDeformableMultiBodyDynamicsWorld.h
	${BulletSoftBody_SRC_DIR}/btDeformableContactConstraint.h
	${BulletSoftBody_SRC_DIR}/btKrylovSolver.h
	${BulletSoftBody_SRC_DIR}/poly34.h

	${BulletSoftBody_SRC_DIR}/btSoftBodySolverVertexBuffer.h
)

ADD_LIBRARY(BulletSoftBody STATIC  ${BulletSoftBody_SRCS} ${BulletSoftBody_HDRS})
target_include_directories(BulletSoftBody 
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/bullet3/src
		${BulletSoftBody_SRC_DIR}
	PUBLIC 
		${BulletSoftBody_SRC_DIR}
		$<BUILD_INTERFACE:${BulletSoftBody_SRC_DIR}>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(BulletSoftBody PROPERTIES SOURCE_DIR ${BulletSoftBody_SRC_DIR})
SET_TARGET_PROPERTIES(BulletSoftBody PROPERTIES VERSION ${BULLET_VERSION})
SET_TARGET_PROPERTIES(BulletSoftBody PROPERTIES SOVERSION ${BULLET_VERSION})
TARGET_LINK_LIBRARIES(BulletSoftBody PUBLIC BulletDynamics)

#BulletInverseDynamics

SET(BulletInverseDynamics_SRCS
	${BulletInverseDynamics_SRC_DIR}/IDMath.cpp
	${BulletInverseDynamics_SRC_DIR}/MultiBodyTree.cpp
	${BulletInverseDynamics_SRC_DIR}/details/MultiBodyTreeInitCache.cpp
	${BulletInverseDynamics_SRC_DIR}/details/MultiBodyTreeImpl.cpp
)

SET(BulletInverseDynamicsRoot_HDRS
	${BulletInverseDynamics_SRC_DIR}/IDConfig.hpp
	${BulletInverseDynamics_SRC_DIR}/IDConfigEigen.hpp
	${BulletInverseDynamics_SRC_DIR}/IDMath.hpp
	${BulletInverseDynamics_SRC_DIR}/IDConfigBuiltin.hpp
	${BulletInverseDynamics_SRC_DIR}/IDErrorMessages.hpp
	${BulletInverseDynamics_SRC_DIR}/MultiBodyTree.hpp
)
SET(BulletInverseDynamicsDetails_HDRS
	${BulletInverseDynamics_SRC_DIR}/details/IDEigenInterface.hpp
	${BulletInverseDynamics_SRC_DIR}/details/IDMatVec.hpp
	${BulletInverseDynamics_SRC_DIR}/details/IDLinearMathInterface.hpp
	${BulletInverseDynamics_SRC_DIR}/details/MultiBodyTreeImpl.hpp
	${BulletInverseDynamics_SRC_DIR}/details/MultiBodyTreeInitCache.hpp
)

SET(BulletInverseDynamics_HDRS
	${BulletInverseDynamicsRoot_HDRS}
	${BulletInverseDynamicsDetails_HDRS}
)

ADD_LIBRARY(BulletInverseDynamics STATIC ${BulletInverseDynamics_SRCS} ${BulletInverseDynamics_HDRS})
target_include_directories(BulletInverseDynamics 
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/bullet3/src
		${BulletInverseDynamics_SRC_DIR}
	PUBLIC 
		${BulletInverseDynamics_SRC_DIR}
		$<BUILD_INTERFACE:${BulletInverseDynamics_SRC_DIR}>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(BulletInverseDynamics PROPERTIES SOURCE_DIR ${BulletInverseDynamics_SRC_DIR})
SET_TARGET_PROPERTIES(BulletInverseDynamics PROPERTIES VERSION ${BULLET_VERSION})
SET_TARGET_PROPERTIES(BulletInverseDynamics PROPERTIES SOVERSION ${BULLET_VERSION})
TARGET_LINK_LIBRARIES(BulletInverseDynamics PUBLIC Bullet3Common LinearMath)

#BulletDynamics

SET(BulletDynamics_SRCS
	${BulletDynamics_SRC_DIR}/Character/btKinematicCharacterController.cpp
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btConeTwistConstraint.cpp
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btContactConstraint.cpp
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btFixedConstraint.cpp
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btGearConstraint.cpp
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btGeneric6DofConstraint.cpp
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btGeneric6DofSpringConstraint.cpp
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btGeneric6DofSpring2Constraint.cpp
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btHinge2Constraint.cpp
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btHingeConstraint.cpp
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btPoint2PointConstraint.cpp
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btSequentialImpulseConstraintSolver.cpp
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btSequentialImpulseConstraintSolverMt.cpp
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btBatchedConstraints.cpp
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btNNCGConstraintSolver.cpp
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btSliderConstraint.cpp
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btSolve2LinearConstraint.cpp
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btTypedConstraint.cpp
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btUniversalConstraint.cpp
	${BulletDynamics_SRC_DIR}/Dynamics/btDiscreteDynamicsWorld.cpp
	${BulletDynamics_SRC_DIR}/Dynamics/btDiscreteDynamicsWorldMt.cpp
	${BulletDynamics_SRC_DIR}/Dynamics/btSimulationIslandManagerMt.cpp
	${BulletDynamics_SRC_DIR}/Dynamics/btRigidBody.cpp
	${BulletDynamics_SRC_DIR}/Dynamics/btSimpleDynamicsWorld.cpp
#	${BulletDynamics_SRC_DIR}/Dynamics/Bullet-C-API.cpp
	${BulletDynamics_SRC_DIR}/Vehicle/btRaycastVehicle.cpp
	${BulletDynamics_SRC_DIR}/Vehicle/btWheelInfo.cpp
	${BulletDynamics_SRC_DIR}/Featherstone/btMultiBody.cpp
	${BulletDynamics_SRC_DIR}/Featherstone/btMultiBodyConstraint.cpp
	${BulletDynamics_SRC_DIR}/Featherstone/btMultiBodyConstraintSolver.cpp
	${BulletDynamics_SRC_DIR}/Featherstone/btMultiBodyDynamicsWorld.cpp
	${BulletDynamics_SRC_DIR}/Featherstone/btMultiBodyFixedConstraint.cpp
	${BulletDynamics_SRC_DIR}/Featherstone/btMultiBodyGearConstraint.cpp
	${BulletDynamics_SRC_DIR}/Featherstone/btMultiBodyJointLimitConstraint.cpp
	${BulletDynamics_SRC_DIR}/Featherstone/btMultiBodyJointMotor.cpp
	${BulletDynamics_SRC_DIR}/Featherstone/btMultiBodyMLCPConstraintSolver.cpp
	${BulletDynamics_SRC_DIR}/Featherstone/btMultiBodyPoint2Point.cpp
	${BulletDynamics_SRC_DIR}/Featherstone/btMultiBodySliderConstraint.cpp
	${BulletDynamics_SRC_DIR}/Featherstone/btMultiBodySphericalJointMotor.cpp
	${BulletDynamics_SRC_DIR}/MLCPSolvers/btDantzigLCP.cpp
	${BulletDynamics_SRC_DIR}/MLCPSolvers/btMLCPSolver.cpp
	${BulletDynamics_SRC_DIR}/MLCPSolvers/btLemkeAlgorithm.cpp
)

SET(Root_HDRS
	${BulletDynamics_SRC_DIR}/../btBulletDynamicsCommon.h
	${BulletDynamics_SRC_DIR}/../btBulletCollisionCommon.h
)
SET(ConstraintSolver_HDRS
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btConeTwistConstraint.h
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btConstraintSolver.h
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btContactConstraint.h
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btContactSolverInfo.h
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btFixedConstraint.h
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btGearConstraint.h
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btGeneric6DofConstraint.h
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btGeneric6DofSpringConstraint.h
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btGeneric6DofSpring2Constraint.h
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btHinge2Constraint.h
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btHingeConstraint.h
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btJacobianEntry.h
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btPoint2PointConstraint.h
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btSequentialImpulseConstraintSolver.h
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btSequentialImpulseConstraintSolverMt.h
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btNNCGConstraintSolver.h
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btSliderConstraint.h
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btSolve2LinearConstraint.h
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btSolverBody.h
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btSolverConstraint.h
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btTypedConstraint.h
	${BulletDynamics_SRC_DIR}/ConstraintSolver/btUniversalConstraint.h
)
SET(Dynamics_HDRS
	${BulletDynamics_SRC_DIR}/Dynamics/btActionInterface.h
	${BulletDynamics_SRC_DIR}/Dynamics/btDiscreteDynamicsWorld.h
	${BulletDynamics_SRC_DIR}/Dynamics/btDiscreteDynamicsWorldMt.h
	${BulletDynamics_SRC_DIR}/Dynamics/btSimulationIslandManagerMt.h
	${BulletDynamics_SRC_DIR}/Dynamics/btDynamicsWorld.h
	${BulletDynamics_SRC_DIR}/Dynamics/btSimpleDynamicsWorld.h
	${BulletDynamics_SRC_DIR}/Dynamics/btRigidBody.h
)
SET(Vehicle_HDRS
	${BulletDynamics_SRC_DIR}/Vehicle/btRaycastVehicle.h
	${BulletDynamics_SRC_DIR}/Vehicle/btVehicleRaycaster.h
	${BulletDynamics_SRC_DIR}/Vehicle/btWheelInfo.h
)

SET(Featherstone_HDRS
	${BulletDynamics_SRC_DIR}/Featherstone/btMultiBody.h
	${BulletDynamics_SRC_DIR}/Featherstone/btMultiBodyConstraint.h
	${BulletDynamics_SRC_DIR}/Featherstone/btMultiBodyConstraintSolver.h
	${BulletDynamics_SRC_DIR}/Featherstone/btMultiBodyDynamicsWorld.h
	${BulletDynamics_SRC_DIR}/Featherstone/btMultiBodyFixedConstraint.h
	${BulletDynamics_SRC_DIR}/Featherstone/btMultiBodyGearConstraint.h
	${BulletDynamics_SRC_DIR}/Featherstone/btMultiBodyJointLimitConstraint.h
	${BulletDynamics_SRC_DIR}/Featherstone/btMultiBodyJointMotor.h
	${BulletDynamics_SRC_DIR}/Featherstone/btMultiBodyLink.h
	${BulletDynamics_SRC_DIR}/Featherstone/btMultiBodyLinkCollider.h
	${BulletDynamics_SRC_DIR}/Featherstone/btMultiBodyMLCPConstraintSolver.h
	${BulletDynamics_SRC_DIR}/Featherstone/btMultiBodyPoint2Point.h
	${BulletDynamics_SRC_DIR}/Featherstone/btMultiBodySliderConstraint.h
	${BulletDynamics_SRC_DIR}/Featherstone/btMultiBodySolverConstraint.h
)

SET(MLCPSolvers_HDRS
	${BulletDynamics_SRC_DIR}/MLCPSolvers/btDantzigLCP.h
	${BulletDynamics_SRC_DIR}/MLCPSolvers/btDantzigSolver.h
	${BulletDynamics_SRC_DIR}/MLCPSolvers/btMLCPSolver.h
	${BulletDynamics_SRC_DIR}/MLCPSolvers/btMLCPSolverInterface.h
	${BulletDynamics_SRC_DIR}/MLCPSolvers/btPATHSolver.h
	${BulletDynamics_SRC_DIR}/MLCPSolvers/btSolveProjectedGaussSeidel.h
	${BulletDynamics_SRC_DIR}/MLCPSolvers/btLemkeSolver.h
	${BulletDynamics_SRC_DIR}/MLCPSolvers/btLemkeAlgorithm.h
)

SET(Character_HDRS
	${BulletDynamics_SRC_DIR}/Character/btCharacterControllerInterface.h
	${BulletDynamics_SRC_DIR}/Character/btKinematicCharacterController.h
)



SET(BulletDynamics_HDRS
	${Root_HDRS}
	${ConstraintSolver_HDRS}
	${Dynamics_HDRS}
	${Vehicle_HDRS}
	${Character_HDRS}
	${Featherstone_HDRS}
	${MLCPSolvers_HDRS}
)


ADD_LIBRARY(BulletDynamics STATIC ${BulletDynamics_SRCS} ${BulletDynamics_HDRS})
target_include_directories(BulletDynamics 
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/bullet3/src
		${BulletDynamics_SRC_DIR}
	PUBLIC 
		${BulletDynamics_SRC_DIR}
		$<BUILD_INTERFACE:${BulletDynamics_SRC_DIR}>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(BulletDynamics PROPERTIES SOURCE_DIR ${BulletDynamics_SRC_DIR})
SET_TARGET_PROPERTIES(BulletDynamics PROPERTIES VERSION ${BULLET_VERSION})
SET_TARGET_PROPERTIES(BulletDynamics PROPERTIES SOVERSION ${BULLET_VERSION})
TARGET_LINK_LIBRARIES(BulletDynamics PUBLIC BulletCollision LinearMath)

# BulletCollision

SET(BulletCollision_SRCS
	${BulletCollision_SRC_DIR}/BroadphaseCollision/btAxisSweep3.cpp
	${BulletCollision_SRC_DIR}/BroadphaseCollision/btBroadphaseProxy.cpp
	${BulletCollision_SRC_DIR}/BroadphaseCollision/btCollisionAlgorithm.cpp
	${BulletCollision_SRC_DIR}/BroadphaseCollision/btDbvt.cpp
	${BulletCollision_SRC_DIR}/BroadphaseCollision/btDbvtBroadphase.cpp
	${BulletCollision_SRC_DIR}/BroadphaseCollision/btDispatcher.cpp
	${BulletCollision_SRC_DIR}/BroadphaseCollision/btOverlappingPairCache.cpp
	${BulletCollision_SRC_DIR}/BroadphaseCollision/btQuantizedBvh.cpp
	${BulletCollision_SRC_DIR}/BroadphaseCollision/btSimpleBroadphase.cpp
	${BulletCollision_SRC_DIR}/CollisionDispatch/btActivatingCollisionAlgorithm.cpp
	${BulletCollision_SRC_DIR}/CollisionDispatch/btBoxBoxCollisionAlgorithm.cpp
	${BulletCollision_SRC_DIR}/CollisionDispatch/btBox2dBox2dCollisionAlgorithm.cpp
	${BulletCollision_SRC_DIR}/CollisionDispatch/btBoxBoxDetector.cpp
	${BulletCollision_SRC_DIR}/CollisionDispatch/btCollisionDispatcher.cpp
	${BulletCollision_SRC_DIR}/CollisionDispatch/btCollisionDispatcherMt.cpp
	${BulletCollision_SRC_DIR}/CollisionDispatch/btCollisionObject.cpp
	${BulletCollision_SRC_DIR}/CollisionDispatch/btCollisionWorld.cpp
	${BulletCollision_SRC_DIR}/CollisionDispatch/btCollisionWorldImporter.cpp
	${BulletCollision_SRC_DIR}/CollisionDispatch/btCompoundCollisionAlgorithm.cpp
	${BulletCollision_SRC_DIR}/CollisionDispatch/btCompoundCompoundCollisionAlgorithm.cpp
	${BulletCollision_SRC_DIR}/CollisionDispatch/btConvexConcaveCollisionAlgorithm.cpp
	${BulletCollision_SRC_DIR}/CollisionDispatch/btConvexConvexAlgorithm.cpp
	${BulletCollision_SRC_DIR}/CollisionDispatch/btConvexPlaneCollisionAlgorithm.cpp
	${BulletCollision_SRC_DIR}/CollisionDispatch/btConvex2dConvex2dAlgorithm.cpp
	${BulletCollision_SRC_DIR}/CollisionDispatch/btDefaultCollisionConfiguration.cpp
	${BulletCollision_SRC_DIR}/CollisionDispatch/btEmptyCollisionAlgorithm.cpp
	${BulletCollision_SRC_DIR}/CollisionDispatch/btGhostObject.cpp
	${BulletCollision_SRC_DIR}/CollisionDispatch/btHashedSimplePairCache.cpp
	${BulletCollision_SRC_DIR}/CollisionDispatch/btInternalEdgeUtility.cpp
	${BulletCollision_SRC_DIR}/CollisionDispatch/btInternalEdgeUtility.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/btManifoldResult.cpp
	${BulletCollision_SRC_DIR}/CollisionDispatch/btSimulationIslandManager.cpp
	${BulletCollision_SRC_DIR}/CollisionDispatch/btSphereBoxCollisionAlgorithm.cpp
	${BulletCollision_SRC_DIR}/CollisionDispatch/btSphereSphereCollisionAlgorithm.cpp
	${BulletCollision_SRC_DIR}/CollisionDispatch/btSphereTriangleCollisionAlgorithm.cpp
	${BulletCollision_SRC_DIR}/CollisionDispatch/btUnionFind.cpp
	${BulletCollision_SRC_DIR}/CollisionDispatch/SphereTriangleDetector.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btBoxShape.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btBox2dShape.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btBvhTriangleMeshShape.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btCapsuleShape.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btCollisionShape.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btCompoundShape.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btConcaveShape.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btConeShape.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btConvexHullShape.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btConvexInternalShape.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btConvexPointCloudShape.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btConvexPolyhedron.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btConvexShape.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btConvex2dShape.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btConvexTriangleMeshShape.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btCylinderShape.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btEmptyShape.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btHeightfieldTerrainShape.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btMiniSDF.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btMinkowskiSumShape.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btMultimaterialTriangleMeshShape.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btMultiSphereShape.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btOptimizedBvh.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btPolyhedralConvexShape.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btScaledBvhTriangleMeshShape.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btSdfCollisionShape.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btShapeHull.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btSphereShape.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btStaticPlaneShape.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btStridingMeshInterface.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btTetrahedronShape.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btTriangleBuffer.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btTriangleCallback.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btTriangleIndexVertexArray.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btTriangleIndexVertexMaterialArray.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btTriangleMesh.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btTriangleMeshShape.cpp
	${BulletCollision_SRC_DIR}/CollisionShapes/btUniformScalingShape.cpp
	${BulletCollision_SRC_DIR}/Gimpact/btContactProcessing.cpp
	${BulletCollision_SRC_DIR}/Gimpact/btGenericPoolAllocator.cpp
	${BulletCollision_SRC_DIR}/Gimpact/btGImpactBvh.cpp
	${BulletCollision_SRC_DIR}/Gimpact/btGImpactCollisionAlgorithm.cpp
	${BulletCollision_SRC_DIR}/Gimpact/btGImpactQuantizedBvh.cpp
	${BulletCollision_SRC_DIR}/Gimpact/btGImpactShape.cpp
	${BulletCollision_SRC_DIR}/Gimpact/btTriangleShapeEx.cpp
	${BulletCollision_SRC_DIR}/Gimpact/gim_box_set.cpp
	${BulletCollision_SRC_DIR}/Gimpact/gim_contact.cpp
	${BulletCollision_SRC_DIR}/Gimpact/gim_memory.cpp
	${BulletCollision_SRC_DIR}/Gimpact/gim_tri_collision.cpp
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btContinuousConvexCollision.cpp
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btConvexCast.cpp
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btGjkConvexCast.cpp
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btGjkEpa2.cpp
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btGjkEpaPenetrationDepthSolver.cpp
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btGjkPairDetector.cpp
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btMinkowskiPenetrationDepthSolver.cpp
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btPersistentManifold.cpp
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btRaycastCallback.cpp
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btSubSimplexConvexCast.cpp
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btVoronoiSimplexSolver.cpp
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btPolyhedralContactClipping.cpp
)

SET(Root_HDRS
	${BulletCollision_SRC_DIR}/../btBulletCollisionCommon.h
)
SET(BroadphaseCollision_HDRS
    ${BulletCollision_SRC_DIR}/BroadphaseCollision/btAxisSweep3Internal.h
	${BulletCollision_SRC_DIR}/BroadphaseCollision/btAxisSweep3.h
	${BulletCollision_SRC_DIR}/BroadphaseCollision/btBroadphaseInterface.h
	${BulletCollision_SRC_DIR}/BroadphaseCollision/btBroadphaseProxy.h
	${BulletCollision_SRC_DIR}/BroadphaseCollision/btCollisionAlgorithm.h
	${BulletCollision_SRC_DIR}/BroadphaseCollision/btDbvt.h
	${BulletCollision_SRC_DIR}/BroadphaseCollision/btDbvtBroadphase.h
	${BulletCollision_SRC_DIR}/BroadphaseCollision/btDispatcher.h
	${BulletCollision_SRC_DIR}/BroadphaseCollision/btOverlappingPairCache.h
	${BulletCollision_SRC_DIR}/BroadphaseCollision/btOverlappingPairCallback.h
	${BulletCollision_SRC_DIR}/BroadphaseCollision/btQuantizedBvh.h
	${BulletCollision_SRC_DIR}/BroadphaseCollision/btSimpleBroadphase.h
)
SET(CollisionDispatch_HDRS
	${BulletCollision_SRC_DIR}/CollisionDispatch/btActivatingCollisionAlgorithm.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/btBoxBoxCollisionAlgorithm.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/btBox2dBox2dCollisionAlgorithm.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/btBoxBoxDetector.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/btCollisionConfiguration.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/btCollisionCreateFunc.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/btCollisionDispatcher.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/btCollisionDispatcherMt.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/btCollisionObject.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/btCollisionObjectWrapper.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/btCollisionWorld.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/btCollisionWorldImporter.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/btCompoundCollisionAlgorithm.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/btCompoundCompoundCollisionAlgorithm.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/btConvexConcaveCollisionAlgorithm.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/btConvexConvexAlgorithm.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/btConvex2dConvex2dAlgorithm.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/btConvexPlaneCollisionAlgorithm.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/btDefaultCollisionConfiguration.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/btEmptyCollisionAlgorithm.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/btGhostObject.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/btHashedSimplePairCache.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/btManifoldResult.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/btSimulationIslandManager.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/btSphereBoxCollisionAlgorithm.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/btSphereSphereCollisionAlgorithm.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/btSphereTriangleCollisionAlgorithm.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/btUnionFind.h
	${BulletCollision_SRC_DIR}/CollisionDispatch/SphereTriangleDetector.h
)
SET(CollisionShapes_HDRS
	${BulletCollision_SRC_DIR}/CollisionShapes/btBoxShape.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btBox2dShape.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btBvhTriangleMeshShape.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btCapsuleShape.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btCollisionMargin.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btCollisionShape.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btCompoundShape.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btConcaveShape.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btConeShape.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btConvexHullShape.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btConvexInternalShape.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btConvexPointCloudShape.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btConvexPolyhedron.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btConvexShape.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btConvex2dShape.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btConvexTriangleMeshShape.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btCylinderShape.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btEmptyShape.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btHeightfieldTerrainShape.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btMaterial.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btMinkowskiSumShape.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btMultimaterialTriangleMeshShape.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btMultiSphereShape.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btOptimizedBvh.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btPolyhedralConvexShape.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btScaledBvhTriangleMeshShape.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btShapeHull.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btSphereShape.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btStaticPlaneShape.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btStridingMeshInterface.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btTetrahedronShape.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btTriangleBuffer.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btTriangleCallback.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btTriangleIndexVertexArray.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btTriangleIndexVertexMaterialArray.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btTriangleInfoMap.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btTriangleMesh.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btTriangleMeshShape.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btTriangleShape.h
	${BulletCollision_SRC_DIR}/CollisionShapes/btUniformScalingShape.h
)
SET(Gimpact_HDRS
	${BulletCollision_SRC_DIR}/Gimpact/btBoxCollision.h
	${BulletCollision_SRC_DIR}/Gimpact/btClipPolygon.h
	${BulletCollision_SRC_DIR}/Gimpact/btContactProcessingStructs.h
	${BulletCollision_SRC_DIR}/Gimpact/btContactProcessing.h
	${BulletCollision_SRC_DIR}/Gimpact/btGenericPoolAllocator.h
	${BulletCollision_SRC_DIR}/Gimpact/btGeometryOperations.h
	${BulletCollision_SRC_DIR}/Gimpact/btGImpactBvhStructs.h
	${BulletCollision_SRC_DIR}/Gimpact/btGImpactBvh.h
	${BulletCollision_SRC_DIR}/Gimpact/btGImpactCollisionAlgorithm.h
	${BulletCollision_SRC_DIR}/Gimpact/btGImpactMassUtil.h
	${BulletCollision_SRC_DIR}/Gimpact/btGImpactQuantizedBvhStructs.h
	${BulletCollision_SRC_DIR}/Gimpact/btGImpactQuantizedBvh.h
	${BulletCollision_SRC_DIR}/Gimpact/btGImpactShape.h
	${BulletCollision_SRC_DIR}/Gimpact/btQuantization.h
	${BulletCollision_SRC_DIR}/Gimpact/btTriangleShapeEx.h
	${BulletCollision_SRC_DIR}/Gimpact/gim_array.h
	${BulletCollision_SRC_DIR}/Gimpact/gim_basic_geometry_operations.h
	${BulletCollision_SRC_DIR}/Gimpact/gim_bitset.h
	${BulletCollision_SRC_DIR}/Gimpact/gim_box_collision.h
	${BulletCollision_SRC_DIR}/Gimpact/gim_box_set.h
	${BulletCollision_SRC_DIR}/Gimpact/gim_clip_polygon.h
	${BulletCollision_SRC_DIR}/Gimpact/gim_contact.h
	${BulletCollision_SRC_DIR}/Gimpact/gim_geom_types.h
	${BulletCollision_SRC_DIR}/Gimpact/gim_geometry.h
	${BulletCollision_SRC_DIR}/Gimpact/gim_hash_table.h
	${BulletCollision_SRC_DIR}/Gimpact/gim_linear_math.h
	${BulletCollision_SRC_DIR}/Gimpact/gim_math.h
	${BulletCollision_SRC_DIR}/Gimpact/gim_memory.h
	${BulletCollision_SRC_DIR}/Gimpact/gim_radixsort.h
	${BulletCollision_SRC_DIR}/Gimpact/gim_tri_collision.h
)
SET(NarrowPhaseCollision_HDRS
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btContinuousConvexCollision.h
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btConvexCast.h
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btConvexPenetrationDepthSolver.h
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btDiscreteCollisionDetectorInterface.h
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btGjkConvexCast.h
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btGjkEpa2.h
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btGjkEpaPenetrationDepthSolver.h
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btGjkPairDetector.h
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btManifoldPoint.h
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btMinkowskiPenetrationDepthSolver.h
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btPersistentManifold.h
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btPointCollector.h
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btRaycastCallback.h
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btSimplexSolverInterface.h
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btSubSimplexConvexCast.h
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btVoronoiSimplexSolver.h
	${BulletCollision_SRC_DIR}/NarrowPhaseCollision/btPolyhedralContactClipping.h
)

SET(BulletCollision_HDRS
	${Root_HDRS}
	${BroadphaseCollision_HDRS}
	${CollisionDispatch_HDRS}
	${CollisionShapes_HDRS}
	${Gimpact_HDRS}
	${NarrowPhaseCollision_HDRS}
)


ADD_LIBRARY(BulletCollision STATIC ${BulletCollision_SRCS} ${BulletCollision_HDRS})
target_include_directories(BulletCollision 
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/bullet3/src
		${BulletCollision_SRC_DIR}
	PUBLIC 
		${BulletCollision_SRC_DIR}
		$<BUILD_INTERFACE:${BulletCollision_SRC_DIR}>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(BulletCollision PROPERTIES SOURCE_DIR ${BulletCollision_SRC_DIR})
SET_TARGET_PROPERTIES(BulletCollision PROPERTIES VERSION ${BULLET_VERSION})
SET_TARGET_PROPERTIES(BulletCollision PROPERTIES SOVERSION ${BULLET_VERSION})
TARGET_LINK_LIBRARIES(BulletCollision PUBLIC LinearMath)

#Bullet3Serialize

SET(Bullet2FileLoader_SRCS
	${Bullet2FileLoader_SRC_DIR}/b3BulletFile.cpp
	${Bullet2FileLoader_SRC_DIR}/b3Chunk.cpp
	${Bullet2FileLoader_SRC_DIR}/b3DNA.cpp
	${Bullet2FileLoader_SRC_DIR}/b3File.cpp
	${Bullet2FileLoader_SRC_DIR}/b3Serializer.cpp
)

SET(Bullet2FileLoader_HDRS
	${Bullet2FileLoader_SRC_DIR}/b3BulletFile.h
	${Bullet2FileLoader_SRC_DIR}/b3Chunk.h
	${Bullet2FileLoader_SRC_DIR}/b3Common.h
	${Bullet2FileLoader_SRC_DIR}/b3Defines.h
	${Bullet2FileLoader_SRC_DIR}/b3DNA.h
	${Bullet2FileLoader_SRC_DIR}/b3File.h
	${Bullet2FileLoader_SRC_DIR}/b3Serializer.h
	${Bullet2FileLoader_SRC_DIR}/autogenerated/bullet2.h
)

ADD_LIBRARY(Bullet2FileLoader STATIC ${Bullet2FileLoader_SRCS} ${Bullet2FileLoader_HDRS})
target_include_directories(Bullet2FileLoader 
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/bullet3/src
		${Bullet2FileLoader_SRC_DIR}
	PUBLIC 
		${Bullet2FileLoader_SRC_DIR}
		$<BUILD_INTERFACE:${Bullet2FileLoader_SRC_DIR}>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(Bullet2FileLoader PROPERTIES SOURCE_DIR ${Bullet2FileLoader_SRC_DIR})
target_link_libraries(Bullet2FileLoader PUBLIC Bullet3Common)
SET_TARGET_PROPERTIES(Bullet2FileLoader PROPERTIES VERSION ${BULLET_VERSION})
SET_TARGET_PROPERTIES(Bullet2FileLoader PROPERTIES SOVERSION ${BULLET_VERSION})

# Bullet3OpenCL

ADD_DEFINITIONS(-DB3_USE_CLEW)

SET(Bullet3OpenCL_clew_SRCS
	${Bullet3OpenCL_clew_SRC_DIR}/../clew/clew.c
	${Bullet3OpenCL_clew_SRC_DIR}/BroadphaseCollision/b3GpuGridBroadphase.cpp
	${Bullet3OpenCL_clew_SRC_DIR}/BroadphaseCollision/b3GpuSapBroadphase.cpp
	${Bullet3OpenCL_clew_SRC_DIR}/BroadphaseCollision/b3GpuParallelLinearBvhBroadphase.cpp
	${Bullet3OpenCL_clew_SRC_DIR}/BroadphaseCollision/b3GpuParallelLinearBvh.cpp
	${Bullet3OpenCL_clew_SRC_DIR}/Initialize/b3OpenCLUtils.cpp
	${Bullet3OpenCL_clew_SRC_DIR}/NarrowphaseCollision/b3ContactCache.cpp
	${Bullet3OpenCL_clew_SRC_DIR}/NarrowphaseCollision/b3ConvexHullContact.cpp
	${Bullet3OpenCL_clew_SRC_DIR}/NarrowphaseCollision/b3GjkEpa.cpp
	${Bullet3OpenCL_clew_SRC_DIR}/NarrowphaseCollision/b3OptimizedBvh.cpp
	${Bullet3OpenCL_clew_SRC_DIR}/NarrowphaseCollision/b3QuantizedBvh.cpp
	${Bullet3OpenCL_clew_SRC_DIR}/NarrowphaseCollision/b3StridingMeshInterface.cpp
	${Bullet3OpenCL_clew_SRC_DIR}/NarrowphaseCollision/b3TriangleCallback.cpp
	${Bullet3OpenCL_clew_SRC_DIR}/NarrowphaseCollision/b3TriangleIndexVertexArray.cpp
	${Bullet3OpenCL_clew_SRC_DIR}/NarrowphaseCollision/b3VoronoiSimplexSolver.cpp
	${Bullet3OpenCL_clew_SRC_DIR}/ParallelPrimitives/b3BoundSearchCL.cpp
	${Bullet3OpenCL_clew_SRC_DIR}/ParallelPrimitives/b3FillCL.cpp
	${Bullet3OpenCL_clew_SRC_DIR}/ParallelPrimitives/b3LauncherCL.cpp
	${Bullet3OpenCL_clew_SRC_DIR}/ParallelPrimitives/b3PrefixScanCL.cpp
	${Bullet3OpenCL_clew_SRC_DIR}/ParallelPrimitives/b3PrefixScanFloat4CL.cpp
	${Bullet3OpenCL_clew_SRC_DIR}/ParallelPrimitives/b3RadixSort32CL.cpp
	${Bullet3OpenCL_clew_SRC_DIR}/Raycast/b3GpuRaycast.cpp
	${Bullet3OpenCL_clew_SRC_DIR}/RigidBody/b3GpuGenericConstraint.cpp
	${Bullet3OpenCL_clew_SRC_DIR}/RigidBody/b3GpuJacobiContactSolver.cpp
	${Bullet3OpenCL_clew_SRC_DIR}/RigidBody/b3GpuNarrowPhase.cpp
	${Bullet3OpenCL_clew_SRC_DIR}/RigidBody/b3GpuPgsConstraintSolver.cpp
	${Bullet3OpenCL_clew_SRC_DIR}/RigidBody/b3GpuPgsContactSolver.cpp
	${Bullet3OpenCL_clew_SRC_DIR}/RigidBody/b3GpuRigidBodyPipeline.cpp
	${Bullet3OpenCL_clew_SRC_DIR}/RigidBody/b3Solver.cpp
)


SET(Bullet3OpenCL_clew_HDRS
#	${Root_HDRS}
)

ADD_LIBRARY(Bullet3OpenCL_clew STATIC ${Bullet3OpenCL_clew_SRCS} ${Bullet3OpenCL_clew_HDRS})
target_include_directories(Bullet3OpenCL_clew 
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/bullet3/src
		${Bullet3OpenCL_clew_SRC_DIR}
	PUBLIC 
		${Bullet3OpenCL_clew_SRC_DIR}
		$<BUILD_INTERFACE:${Bullet3OpenCL_clew_SRC_DIR}>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(Bullet3OpenCL_clew PROPERTIES SOURCE_DIR ${Bullet3OpenCL_clew_SRC_DIR})
SET_TARGET_PROPERTIES(Bullet3OpenCL_clew PROPERTIES VERSION ${BULLET_VERSION})
SET_TARGET_PROPERTIES(Bullet3OpenCL_clew PROPERTIES SOVERSION ${BULLET_VERSION})
TARGET_LINK_LIBRARIES(Bullet3OpenCL_clew PUBLIC LinearMath Bullet3Dynamics ${CMAKE_DL_LIBS})

# Bullet3Geometry

SET(Bullet3Geometry_SRCS
	${Bullet3Geometry_SRC_DIR}/b3ConvexHullComputer.cpp
	${Bullet3Geometry_SRC_DIR}/b3GeometryUtil.cpp
)

SET(Bullet3Geometry_HDRS
	${Bullet3Geometry_SRC_DIR}/b3AabbUtil.h
	${Bullet3Geometry_SRC_DIR}/b3ConvexHullComputer.h
	${Bullet3Geometry_SRC_DIR}/b3GeometryUtil.h
	${Bullet3Geometry_SRC_DIR}/b3GrahamScan2dConvexHull.h
)

ADD_LIBRARY(Bullet3Geometry STATIC ${Bullet3Geometry_SRCS} ${Bullet3Geometry_HDRS})
target_include_directories(Bullet3Geometry 
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/bullet3/src
		${Bullet3Geometry_SRC_DIR}
	PUBLIC 
		${Bullet3Geometry_SRC_DIR}
		$<BUILD_INTERFACE:${Bullet3Geometry_SRC_DIR}>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(Bullet3Geometry PROPERTIES SOURCE_DIR ${Bullet3Geometry_SRC_DIR})
target_link_libraries(Bullet3Geometry PUBLIC Bullet3Common)
SET_TARGET_PROPERTIES(Bullet3Geometry PROPERTIES VERSION ${BULLET_VERSION})
SET_TARGET_PROPERTIES(Bullet3Geometry PROPERTIES SOVERSION ${BULLET_VERSION})

# Bullet3Dynamics

SET(Bullet3Dynamics_SRCS
	${Bullet3Dynamics_SRC_DIR}/b3CpuRigidBodyPipeline.cpp
	${Bullet3Dynamics_SRC_DIR}/ConstraintSolver/b3FixedConstraint.cpp
	${Bullet3Dynamics_SRC_DIR}/ConstraintSolver/b3Generic6DofConstraint.cpp
	${Bullet3Dynamics_SRC_DIR}/ConstraintSolver/b3PgsJacobiSolver.cpp
	${Bullet3Dynamics_SRC_DIR}/ConstraintSolver/b3Point2PointConstraint.cpp
	${Bullet3Dynamics_SRC_DIR}/ConstraintSolver/b3TypedConstraint.cpp
)

SET(Bullet3Dynamics_HDRS
	${Bullet3Dynamics_SRC_DIR}/b3CpuRigidBodyPipeline.h
	${Bullet3Dynamics_SRC_DIR}/ConstraintSolver/b3ContactSolverInfo.h
	${Bullet3Dynamics_SRC_DIR}/ConstraintSolver/b3FixedConstraint.h
	${Bullet3Dynamics_SRC_DIR}/ConstraintSolver/b3Generic6DofConstraint.h
	${Bullet3Dynamics_SRC_DIR}/ConstraintSolver/b3JacobianEntry.h
	${Bullet3Dynamics_SRC_DIR}/ConstraintSolver/b3PgsJacobiSolver.h
	${Bullet3Dynamics_SRC_DIR}/ConstraintSolver/b3Point2PointConstraint.h
	${Bullet3Dynamics_SRC_DIR}/ConstraintSolver/b3SolverBody.h
	${Bullet3Dynamics_SRC_DIR}/ConstraintSolver/b3SolverConstraint.h
	${Bullet3Dynamics_SRC_DIR}/ConstraintSolver/b3TypedConstraint.h
	${Bullet3Dynamics_SRC_DIR}/shared/b3ContactConstraint4.h
	${Bullet3Dynamics_SRC_DIR}/shared/b3ConvertConstraint4.h
	${Bullet3Dynamics_SRC_DIR}/shared/b3Inertia.h
	${Bullet3Dynamics_SRC_DIR}/shared/b3IntegrateTransforms.h
)

ADD_LIBRARY(Bullet3Dynamics STATIC ${Bullet3Dynamics_SRCS} ${Bullet3Dynamics_HDRS})
target_include_directories(Bullet3Dynamics 
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/bullet3/src
		${Bullet3Dynamics_SRC_DIR}
	PUBLIC 
		${Bullet3Dynamics_SRC_DIR}
		$<BUILD_INTERFACE:${Bullet3Dynamics_SRC_DIR}>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(Bullet3Dynamics PROPERTIES SOURCE_DIR ${Bullet3Dynamics_SRC_DIR})
target_link_libraries(Bullet3Dynamics PUBLIC Bullet3Collision)
SET_TARGET_PROPERTIES(Bullet3Dynamics PROPERTIES VERSION ${BULLET_VERSION})
SET_TARGET_PROPERTIES(Bullet3Dynamics PROPERTIES SOVERSION ${BULLET_VERSION})

#Bullet3Common

SET(Bullet3Common_SRCS
	${Bullet3Common_SRC_DIR}/b3AlignedAllocator.cpp
	${Bullet3Common_SRC_DIR}/b3Vector3.cpp
	${Bullet3Common_SRC_DIR}/b3Logging.cpp
)

SET(Bullet3Common_HDRS
	${Bullet3Common_SRC_DIR}/b3AlignedAllocator.h
	${Bullet3Common_SRC_DIR}/b3AlignedObjectArray.h
	${Bullet3Common_SRC_DIR}/b3CommandLineArgs.h
	${Bullet3Common_SRC_DIR}/b3HashMap.h
	${Bullet3Common_SRC_DIR}/b3Logging.h
	${Bullet3Common_SRC_DIR}/b3Matrix3x3.h
	${Bullet3Common_SRC_DIR}/b3MinMax.h
	${Bullet3Common_SRC_DIR}/b3PoolAllocator.h
	${Bullet3Common_SRC_DIR}/b3QuadWord.h
	${Bullet3Common_SRC_DIR}/b3Quaternion.h
	${Bullet3Common_SRC_DIR}/b3Random.h
	${Bullet3Common_SRC_DIR}/b3Scalar.h
	${Bullet3Common_SRC_DIR}/b3StackAlloc.h
	${Bullet3Common_SRC_DIR}/b3Transform.h
	${Bullet3Common_SRC_DIR}/b3TransformUtil.h
	${Bullet3Common_SRC_DIR}/b3Vector3.h
	${Bullet3Common_SRC_DIR}/shared/b3Float4
	${Bullet3Common_SRC_DIR}/shared/b3Int2.h
	${Bullet3Common_SRC_DIR}/shared/b3Int4.h
	${Bullet3Common_SRC_DIR}/shared/b3Mat3x3.h
	${Bullet3Common_SRC_DIR}/shared/b3PlatformDefinitions
	${Bullet3Common_SRC_DIR}/shared/b3Quat.h
)

ADD_LIBRARY(Bullet3Common STATIC ${Bullet3Common_SRCS} ${Bullet3Common_HDRS})
target_include_directories(Bullet3Common 
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/bullet3/src
		${Bullet3Common_SRC_DIR}
	PUBLIC 
		${Bullet3Common_SRC_DIR}
		$<BUILD_INTERFACE:${Bullet3Common_SRC_DIR}>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(Bullet3Common PROPERTIES SOURCE_DIR ${Bullet3Common_SRC_DIR})
SET_TARGET_PROPERTIES(Bullet3Common PROPERTIES VERSION ${BULLET_VERSION})
SET_TARGET_PROPERTIES(Bullet3Common PROPERTIES SOVERSION ${BULLET_VERSION})


SET(Bullet3Collision_SRCS
	${Bullet3Collision_SRC_DIR}/BroadPhaseCollision/b3DynamicBvh.cpp
	${Bullet3Collision_SRC_DIR}/BroadPhaseCollision/b3DynamicBvhBroadphase.cpp
	${Bullet3Collision_SRC_DIR}/BroadPhaseCollision/b3OverlappingPairCache.cpp
	${Bullet3Collision_SRC_DIR}/NarrowPhaseCollision/b3ConvexUtility.cpp
	${Bullet3Collision_SRC_DIR}/NarrowPhaseCollision/b3CpuNarrowPhase.cpp
)

SET(Bullet3CollisionBroadPhase_HDRS
	${Bullet3Collision_SRC_DIR}/BroadPhaseCollision/b3BroadphaseCallback.h
	${Bullet3Collision_SRC_DIR}/BroadPhaseCollision/b3DynamicBvh.h
	${Bullet3Collision_SRC_DIR}/BroadPhaseCollision/b3DynamicBvhBroadphase.h
	${Bullet3Collision_SRC_DIR}/BroadPhaseCollision/b3OverlappingPair.h
	${Bullet3Collision_SRC_DIR}/BroadPhaseCollision/b3OverlappingPairCache.h
)
SET(Bullet3CollisionBroadPhaseShared_HDRS
	${Bullet3Collision_SRC_DIR}/BroadPhaseCollision/shared/b3Aabb.h
)

SET(Bullet3CollisionNarrowPhase_HDRS
	${Bullet3Collision_SRC_DIR}/NarrowPhaseCollision/b3Config.h
	${Bullet3Collision_SRC_DIR}/NarrowPhaseCollision/b3Contact4.h
	${Bullet3Collision_SRC_DIR}/NarrowPhaseCollision/b3ConvexUtility.h
	${Bullet3Collision_SRC_DIR}/NarrowPhaseCollision/b3CpuNarrowPhase.h
	${Bullet3Collision_SRC_DIR}/NarrowPhaseCollision/b3RaycastInfo.h
	${Bullet3Collision_SRC_DIR}/NarrowPhaseCollision/b3RigidBodyCL.h
)
SET(Bullet3CollisionNarrowPhaseShared_HDRS
	${Bullet3Collision_SRC_DIR}/NarrowPhaseCollision/shared/b3BvhSubtreeInfoData.h
	${Bullet3Collision_SRC_DIR}/NarrowPhaseCollision/shared/b3BvhTraversal.h
	${Bullet3Collision_SRC_DIR}/NarrowPhaseCollision/shared/b3ClipFaces.h
	${Bullet3Collision_SRC_DIR}/NarrowPhaseCollision/shared/b3Collidable.h
	${Bullet3Collision_SRC_DIR}/NarrowPhaseCollision/shared/b3Contact4Data.h
	${Bullet3Collision_SRC_DIR}/NarrowPhaseCollision/shared/b3ContactConvexConvexSAT.h
	${Bullet3Collision_SRC_DIR}/NarrowPhaseCollision/shared/b3ContactSphereSphere.h
	${Bullet3Collision_SRC_DIR}/NarrowPhaseCollision/shared/b3ConvexPolyhedronData.h
	${Bullet3Collision_SRC_DIR}/NarrowPhaseCollision/shared/b3FindConcaveSatAxis.h
	${Bullet3Collision_SRC_DIR}/NarrowPhaseCollision/shared/b3FindSeparatingAxis.h
	${Bullet3Collision_SRC_DIR}/NarrowPhaseCollision/shared/b3MprPenetration.h
	${Bullet3Collision_SRC_DIR}/NarrowPhaseCollision/shared/b3NewContactReduction.h
	${Bullet3Collision_SRC_DIR}/NarrowPhaseCollision/shared/b3QuantizedBvhNodeData.h
	${Bullet3Collision_SRC_DIR}/NarrowPhaseCollision/shared/b3ReduceContacts.h
	${Bullet3Collision_SRC_DIR}/NarrowPhaseCollision/shared/b3RigidBodyData.h
	${Bullet3Collision_SRC_DIR}/NarrowPhaseCollision/shared/b3UpdateAabbs.h
)

SET(Bullet3Collision_HDRS
	${Bullet3CollisionBroadPhase_HDRS}
	${Bullet3CollisionBroadPhaseShared_HDRS}
	${Bullet3CollisionNarrowPhaseShared_HDRS}
	${Bullet3CollisionNarrowPhase_HDRS}
)

ADD_LIBRARY(Bullet3Collision STATIC ${Bullet3Collision_SRCS} ${Bullet3Collision_HDRS})
target_include_directories(Bullet3Collision 
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/bullet3/src
		${Bullet3Collision_SRC_DIR}
	PUBLIC 
		${Bullet3Collision_SRC_DIR}
		$<BUILD_INTERFACE:${Bullet3Collision_SRC_DIR}>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(Bullet3Collision PROPERTIES SOURCE_DIR ${Bullet3Collision_SRC_DIR})
target_link_libraries(Bullet3Collision PUBLIC Bullet3Geometry)
SET_TARGET_PROPERTIES(Bullet3Collision PROPERTIES VERSION ${BULLET_VERSION})
SET_TARGET_PROPERTIES(Bullet3Collision PROPERTIES SOVERSION ${BULLET_VERSION})

# Extras/InverseDynamics

ADD_LIBRARY(BulletInverseDynamicsUtils STATIC
	${InverseDynamics_SRC_DIR}/CloneTreeCreator.cpp
	${InverseDynamics_SRC_DIR}/CoilCreator.cpp
	${InverseDynamics_SRC_DIR}/MultiBodyTreeCreator.cpp
	${InverseDynamics_SRC_DIR}/btMultiBodyTreeCreator.cpp
	${InverseDynamics_SRC_DIR}/DillCreator.cpp
	${InverseDynamics_SRC_DIR}/MultiBodyTreeDebugGraph.cpp
	${InverseDynamics_SRC_DIR}/invdyn_bullet_comparison.cpp
	${InverseDynamics_SRC_DIR}/IDRandomUtil.cpp
	${InverseDynamics_SRC_DIR}/RandomTreeCreator.cpp
	${InverseDynamics_SRC_DIR}/SimpleTreeCreator.cpp
	${InverseDynamics_SRC_DIR}/MultiBodyNameMap.cpp
	${InverseDynamics_SRC_DIR}/User2InternalIndex.cpp
)

target_include_directories(BulletInverseDynamicsUtils 
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/bullet3/src
		${InverseDynamics_SRC_DIR}
	PUBLIC 
		${InverseDynamics_SRC_DIR}
		$<BUILD_INTERFACE:${InverseDynamics_SRC_DIR}>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(BulletInverseDynamicsUtils PROPERTIES SOURCE_DIR ${InverseDynamics_SRC_DIR})
SET_TARGET_PROPERTIES(BulletInverseDynamicsUtils PROPERTIES VERSION ${BULLET_VERSION})
SET_TARGET_PROPERTIES(BulletInverseDynamicsUtils PROPERTIES SOVERSION ${BULLET_VERSION})
TARGET_LINK_LIBRARIES(BulletInverseDynamicsUtils PUBLIC BulletInverseDynamics BulletDynamics BulletCollision Bullet3Common LinearMath)

