cmake_minimum_required(VERSION 3.16)

project (libmongoc C)

set(libmongoc_path "mongo-c-driver/src/libmongoc")

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} mongo-c-driver/src/libmongoc/build/cmake)

include (InstallRequiredSystemLibraries)
include (CheckStructHasMember)

message ("libmongoc version (from VERSION_CURRENT file): ${MONGOC_VERSION}")

# Defaults.
set (MONGOC_ENABLE_SSL 0)
set (MONGOC_ENABLE_SSL_OPENSSL 0)
set (MONGOC_HAVE_ASN1_STRING_GET0_DATA 0)
set (MONGOC_ENABLE_SSL_LIBRESSL 0)
set (MONGOC_ENABLE_SSL_SECURE_TRANSPORT 0)
set (MONGOC_ENABLE_SSL_SECURE_CHANNEL 0)

set (MONGOC_ENABLE_CRYPTO 0)
set (MONGOC_ENABLE_CRYPTO_LIBCRYPTO 0)
set (MONGOC_ENABLE_CRYPTO_COMMON_CRYPTO 0)
set (MONGOC_ENABLE_CRYPTO_CNG 0)

set (MONGOC_ENABLE_CRYPTO_SYSTEM_PROFILE 0)

set (MONGOC_ENABLE_COMPRESSION 0)
set (MONGOC_ENABLE_COMPRESSION_SNAPPY 0)
set (MONGOC_ENABLE_COMPRESSION_ZLIB 0)
set (MONGOC_ENABLE_COMPRESSION_ZSTD 0)

set (MONGOC_OUTPUT_BASENAME "mongoc" CACHE STRING "Output mongoc library base name")

if (ENABLE_COVERAGE)
   set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g --coverage")
endif ()

if (NOT ENABLE_ZLIB MATCHES "SYSTEM|AUTO|BUNDLED|OFF")
   message (FATAL_ERROR
      "ENABLE_ZLIB option must be SYSTEM, BUNDLED, AUTO, or OFF"
      )
endif ()

if (NOT ENABLE_ZSTD MATCHES "ON|AUTO|OFF")
   message (FATAL_ERROR "ENABLE_ZSTD option must be ON, AUTO, or OFF")
endif ()

# Disable warnings on bundled zlib source files.
set_source_files_properties (${ZLIB_SOURCES} PROPERTIES COMPILE_FLAGS -w)

# Copy zconf.h.in to zconf.h; even when using system zlib, the 'dist' target
# will look for zconf.h in that location.
configure_file (
   "mongo-c-driver/src/zlib-1.2.11/zconf.h.in"
   "${CMAKE_BINARY_DIR}/src/zlib-1.2.11/zconf.h"
   COPYONLY
)
if (ENABLE_ZLIB MATCHES "SYSTEM|AUTO")
   message (STATUS "Searching for zlib CMake packages")
   include (FindZLIB)
   if (ZLIB_FOUND)
      message ("--   zlib found version \"${ZLIB_VERSION_STRING}\"")
      message ("--   zlib include path \"${ZLIB_INCLUDE_DIRS}\"")
      message ("--   zlib libraries \"${ZLIB_LIBRARIES}\"")
   else ()
      if (ENABLE_ZLIB STREQUAL "SYSTEM")
         message (FATAL_ERROR
            "Unable to find system zlib package. Either specify the zlib \
            location by setting ZLIB_ROOT, or else set ENABLE_ZLIB=BUNDLED or \
            set ENABLE_ZLIB=OFF."
            )
      endif ()
      set (ZLIB_LIBRARIES "")
   endif ()
endif ()

set (PRIVATE_ZLIB_INCLUDES "")
if ( (ENABLE_ZLIB STREQUAL "BUNDLED")
   OR (ENABLE_ZLIB STREQUAL "AUTO" AND NOT ZLIB_FOUND) )
   message (STATUS "Enabling zlib compression (bundled)")
   set (SOURCES ${SOURCES} ${ZLIB_SOURCES})
   set (
      PRIVATE_ZLIB_INCLUDES
      "mongo-c-driver/src/zlib-1.2.11"
      "${CMAKE_BINARY_DIR}/src/zlib-1.2.11"
   )
endif ()

if (NOT ENABLE_ZLIB STREQUAL "OFF")
   # At this point the system zlib was found, or the bundled library was used
   include (CheckIncludeFiles)
   check_include_files ("unistd.h" HAVE_UNISTD_H)
   check_include_files ("stdarg.h" HAVE_STDARG_H)
   set (MONGOC_ENABLE_COMPRESSION 1)
   set (MONGOC_ENABLE_COMPRESSION_ZLIB 1)
else ()
   message (STATUS "Disabling zlib compression")
endif ()


if (NOT ENABLE_ZSTD STREQUAL OFF)
   message (STATUS "Searching for compression library zstd")
   find_package(PkgConfig)
   pkg_check_modules (ZSTD libzstd)

   if (NOT ZSTD_FOUND)
      if (ENABLE_ZSTD MATCHES "ON")
         message (FATAL_ERROR "  Not found")
      else ()
         message (STATUS "  Not found")
      endif ()
   # The compression format below this version isn't supported. See SERVER-43070
   elseif (${ZSTD_VERSION} VERSION_LESS "0.8.0")
      if (ENABLE_ZSTD MATCHES "ON")
         message (FATAL_ERROR "Detected zstd version ${ZSTD_VERSION} but version 0.8.0 required")
      else ()
         message (STATUS "Detected zstd version ${ZSTD_VERSION} but version 0.8.0 required")
      endif ()
   else ()
      message (STATUS "  Found zstd version ${ZSTD_VERSION} in ${ZSTD_INCLUDE_DIRS}")
      set (MONGOC_ENABLE_COMPRESSION 1)
      set (MONGOC_ENABLE_COMPRESSION_ZSTD 1)

      include_directories (${ZSTD_INCLUDE_DIRS})
      if (${CMAKE_VERSION} VERSION_LESS "3.12.0")
         set (MONGOC_ZSTD_LIBRARIES ${ZSTD_LIBRARIES})
      else ()
         set (MONGOC_ZSTD_LIBRARIES ${ZSTD_LINK_LIBRARIES})
      endif ()
   endif()
endif()

if (NOT ENABLE_SSL STREQUAL OFF)
   # Try OpenSSL automatically everywhere but Mac and Windows.
   if (ENABLE_SSL STREQUAL "OPENSSL"
       OR (NOT APPLE AND NOT WIN32 AND ENABLE_SSL STREQUAL "AUTO"))
      # Sets OPENSSL_FOUND on success.
      include (FindOpenSSL)
   endif ()

   if (ENABLE_SSL STREQUAL LIBRESSL)
      include (FindPkgConfig)
      message ("-- Searching for LibreSSL/libtls")
      pkg_check_modules (LIBRESSL libtls)
      if (LIBRESSL_FOUND)
         message ("--   Found ${LIBRESSL_LIBRARIES}")
         set (SSL_LIBRARIES ${LIBRESSL_LIBRARIES})
         if (LIBRESSL_INCLUDE_DIRS)
           include_directories ("${LIBRESSL_INCLUDE_DIRS}")
         endif ()
         link_directories ("${LIBRESSL_LIBRARY_DIRS}")
         set (LIBRESSL 1)
      else ()
         message ("--   Not found")
      endif ()
   endif ()

   if (ENABLE_SSL STREQUAL DARWIN OR (APPLE AND ENABLE_SSL STREQUAL "AUTO"))
      if (APPLE)
         set (SECURE_TRANSPORT 1)
      else ()
         message (FATAL_ERROR "ENABLE_SSL=DARWIN only supported on Mac OS X")
      endif ()
   endif ()

   if (ENABLE_SSL STREQUAL WINDOWS OR (WIN32 AND ENABLE_SSL STREQUAL "AUTO"))
      if (WIN32)
         set (SECURE_CHANNEL 1)
      else ()
         message (FATAL_ERROR "ENABLE_SSL=WINDOWS only supported on Windows")
      endif ()
   endif ()

   if (NOT OPENSSL_FOUND AND NOT SECURE_TRANSPORT AND NOT SECURE_CHANNEL AND NOT LIBRESSL)
      if (ENABLE_SSL STREQUAL AUTO)
         set (ENABLE_SSL OFF)
      else ()
         message (FATAL_ERROR "No SSL library found")
      endif ()
   endif ()
endif ()

if (OPENSSL_FOUND)
   if (WIN32 AND OPENSSL_VERSION GREATER 1.1 AND NOT
         ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER 3.7)
      message (FATAL_ERROR "Building against OpenSSL 1.1.0 and later requires CMake 3.7 or later (hint:"
         " You can also compile against Windows Secure Transport with -DENABLE_SSL=WINDOWS")
   endif ()
   if (APPLE AND NOT OPENSSL_ROOT_DIR)
      message (WARNING "Building with OpenSSL but OPENSSL_ROOT_DIR not defined. If build fails to link"
         " to OpenSSL, define OPENSSL_ROOT_DIR as the path to the OpenSSL installation directory.")
   endif ()
   include (CheckLibraryExists)
   # Check for newer OpenSSL string function.
   check_library_exists ("${OPENSSL_CRYPTO_LIBRARY}"
      ASN1_STRING_get0_data "openssl/asn1.h" HAVE_ASN1_STRING_GET0_DATA
   )
   if (HAVE_ASN1_STRING_GET0_DATA)
      set (MONGOC_HAVE_ASN1_STRING_GET0_DATA 1)
   endif ()
   set (MONGOC_ENABLE_SSL 1)
   set (MONGOC_ENABLE_SSL_OPENSSL 1)
   set (MONGOC_ENABLE_CRYPTO 1)
   set (MONGOC_ENABLE_CRYPTO_LIBCRYPTO 1)
elseif (SECURE_TRANSPORT)
   set (MONGOC_ENABLE_SSL 1)
   set (MONGOC_ENABLE_SSL_SECURE_TRANSPORT 1)
   set (MONGOC_ENABLE_CRYPTO 1)
   set (MONGOC_ENABLE_CRYPTO_COMMON_CRYPTO 1)
elseif (SECURE_CHANNEL)
   set (MONGOC_ENABLE_SSL 1)
   set (MONGOC_ENABLE_SSL_SECURE_CHANNEL 1)
   set (MONGOC_ENABLE_CRYPTO 1)
   set (MONGOC_ENABLE_CRYPTO_CNG 1)
elseif (LIBRESSL)
   set (MONGOC_ENABLE_SSL 1)
   set (MONGOC_ENABLE_SSL_LIBRESSL 1)
   set (MONGOC_ENABLE_CRYPTO 1)
   set (MONGOC_ENABLE_CRYPTO_LIBCRYPTO 1)
endif ()

if (ENABLE_CRYPTO_SYSTEM_PROFILE)
   if (OPENSSL_FOUND)
      set (MONGOC_ENABLE_CRYPTO_SYSTEM_PROFILE 1)
   else ()
      message (FATAL_ERROR "ENABLE_CRYPTO_SYSTEM_PROFILE only available with OpenSSL")
   endif ()
endif ()

if (MONGOC_ENABLE_MONGODB_AWS_AUTH AND ENABLE_SSL STREQUAL OFF)
   message (FATAL_ERROR "ENABLE_MONGODB_AWS_AUTH but no SSL library was found")
endif ()

if (NOT ENABLE_SASL MATCHES "CYRUS|SSPI|AUTO|OFF")
   message (FATAL_ERROR
      "ENABLE_SASL option must be CYRUS, SSPI, AUTO, or OFF")
endif ()

# Defaults.
set (MONGOC_ENABLE_SASL 0)
set (MONGOC_ENABLE_SASL_CYRUS 0)
set (MONGOC_ENABLE_SASL_SSPI 0)
set (MONGOC_HAVE_SASL_CLIENT_DONE 0)

if (NOT ENABLE_SASL STREQUAL OFF)
   if ( (ENABLE_SASL MATCHES "SSPI|AUTO") AND WIN32)
      set (MONGOC_ENABLE_SASL 1)
      set (MONGOC_ENABLE_SASL_SSPI 1)
   elseif (ENABLE_SASL MATCHES "AUTO|CYRUS")
      # Sets SASL_LIBRARIES.
      include (FindSASL2)
      if (SASL_FOUND)
         set (MONGOC_ENABLE_SASL 1)
         set (MONGOC_ENABLE_SASL_CYRUS 1)
      elseif (ENABLE_SASL STREQUAL "CYRUS")
          message (FATAL_ERROR "ENABLE_SASL=CYRUS could not be satisfied")
      endif()
   endif ()
endif ()


if (ENABLE_AUTOMATIC_INIT_AND_CLEANUP)
   set (MONGOC_NO_AUTOMATIC_GLOBALS 0)
else ()
   set (MONGOC_NO_AUTOMATIC_GLOBALS 1)
endif ()

include (CheckTypeSize)
if (WIN32)
   SET (CMAKE_EXTRA_INCLUDE_FILES "ws2tcpip.h")
else ()
   SET (CMAKE_EXTRA_INCLUDE_FILES "sys/socket.h")
endif ()
CHECK_TYPE_SIZE (socklen_t HAVE_SOCKLEN)
SET (CMAKE_EXTRA_INCLUDE_FILES)

if (HAVE_SOCKLEN)
   set (MONGOC_HAVE_SOCKLEN 1)
   set (MONGOC_SOCKET_ARG3 "socklen_t")
else ()
   set (MONGOC_HAVE_SOCKLEN 0)
   set (MONGOC_SOCKET_ARG3 "int")
endif ()

include (mongo-c-driver/build/cmake/FindResSearch.cmake)
include (mongo-c-driver/build/cmake/CheckSchedGetCPU.cmake)

function (mongoc_get_accept_args ARG2 ARG3)
   SET (VAR 0)
   foreach (ARG2_VAL "struct sockaddr" "void")
   foreach (ARG3_VAL "socklen_t" "size_t" "int")

   MATH (EXPR VAR "${VAR}+1")

   FILE (WRITE ${CMAKE_CURRENT_BINARY_DIR}/accept_test${VAR}.c
   "#include <sys/types.h>
    #include <sys/socket.h>

    int main ()
    {
      int a = 0;
      ${ARG2_VAL} *b = 0;
      ${ARG3_VAL} *c = 0;
      accept (a, b, c);
      return 0;
   }
   ")

   TRY_COMPILE (RES ${CMAKE_CURRENT_BINARY_DIR}
   ${CMAKE_CURRENT_BINARY_DIR}/accept_test${VAR}.c CMAKE_FLAGS
   "-Werror -DCMAKE_CXX_LINK_EXECUTABLE='echo not linking now...'" OUTPUT_VARIABLE LOG2)

   if (RES)
      message (
         STATUS
         "Detected parameters: accept (int, ${ARG2_VAL} *, ${ARG3_VAL} *)")

      set (${ARG2} ${ARG2_VAL} PARENT_SCOPE)
      set (${ARG3} ${ARG3_VAL} PARENT_SCOPE)
      return ()
   endif ()

   endforeach ()
   endforeach ()

endfunction ()

# Reasonable defaults.
set (MONGOC_SOCKET_ARG2 "struct sockaddr")
set (MONGOC_SOCKET_ARG3 "socklen_t")

if (NOT WIN32)
   mongoc_get_accept_args (MONGOC_SOCKET_ARG2 MONGOC_SOCKET_ARG3)
endif ()

set (MONGOC_API_VERSION 1.0)

set (CPACK_PACKAGE_VERSION_MAJOR ${MONGOC_MAJOR_VERSION})
set (CPACK_PACKAGE_VERSION_MINOR ${MONGOC_MINOR_VERSION})

set (MONGOC_CC ${CMAKE_C_COMPILER})
set (MONGOC_USER_SET_CFLAGS ${CMAKE_C_FLAGS})
set (MONGOC_USER_SET_LDFLAGS ${CMAKE_EXE_LINKER_FLAGS})

set (MONGOC_TRACE 0)

if (ENABLE_TRACING)
   set (MONGOC_TRACE 1)
endif ()

# Sets SNAPPY_LIBRARIES and SNAPPY_INCLUDE_DIRS.
if (SNAPPY_INCLUDE_DIRS)
   set (MONGOC_ENABLE_COMPRESSION 1)
   include_directories ("${SNAPPY_INCLUDE_DIRS}")
endif ()

set (MONGOC_ENABLE_SHM_COUNTERS 0)

if (NOT ENABLE_SHM_COUNTERS MATCHES "ON|OFF|AUTO")
   message (FATAL_ERROR "ENABLE_SHM_COUNTERS option must be ON, OFF, or AUTO")
endif ()

if (ENABLE_SHM_COUNTERS STREQUAL "AUTO")
   if (UNIX AND NOT APPLE)
      set (ENABLE_SHM_COUNTERS ON)
   endif ()
endif ()

if (ENABLE_SHM_COUNTERS STREQUAL "ON")
   if (APPLE OR NOT UNIX)
      message (
         FATAL_ERROR
         "Shared memory performance counters not supported on Mac or Windows")
   endif ()
   set (MONGOC_ENABLE_SHM_COUNTERS 1)
   find_library(RT_LIBRARY rt HINTS /usr/lib32)
   if (RT_LIBRARY)
      set (SHM_LIBRARIES rt)
   endif ()
endif ()

if (NOT ENABLE_ICU MATCHES "AUTO|ON|OFF")
   message (FATAL_ERROR, "ENABLE_ICU option must be AUTO, ON, or OFF")
endif()

if (NOT ENABLE_ICU STREQUAL OFF)
   if (ENABLE_ICU STREQUAL ON)
      # do not suppress log output if find_package cannot find ICU
      find_package (ICU COMPONENTS uc)
   elseif (ENABLE_ICU STREQUAL AUTO)
      find_package (ICU QUIET COMPONENTS uc)
   endif()
   if (ICU_FOUND)
      set (MONGOC_ENABLE_ICU 1)
      include_directories ("${ICU_INCLUDE_DIR}")
   elseif (ENABLE_ICU STREQUAL ON)
      message (FATAL_ERROR "No ICU library found. If ICU is installed in a non-standard directory, define ICU_ROOT as the ICU installation path.")
   elseif (ENABLE_ICU STREQUAL AUTO)
      message (STATUS "No ICU library found, SASLPrep disabled for SCRAM-SHA-256 authentication.")
      message (STATUS "If ICU is installed in a non-standard directory, define ICU_ROOT as the ICU installation path.")
   endif()
endif()

# Configure client side encryption.
set (MONGOC_ENABLE_CLIENT_SIDE_ENCRYPTION 0)
if (NOT ENABLE_CLIENT_SIDE_ENCRYPTION MATCHES "AUTO|ON|OFF")
   message (FATAL_ERROR, "ENABLE_CLIENT_SIDE_ENCRYPTION option must be AUTO, ON, or OFF")
endif ()

if (NOT MONGOC_ENABLE_SSL)
   if (ENABLE_CLIENT_SIDE_ENCRYPTION STREQUAL ON)
      message (FATAL_ERROR "SSL disabled, but is required for Client-Side Field Level Encryption support.")
   elseif (ENABLE_CLIENT_SIDE_ENCRYPTION STREQUAL AUTO)
      message (STATUS "SSL disabled. Configuring without Client-Side Field Level Encryption support.")
   endif ()
elseif (NOT ENABLE_CLIENT_SIDE_ENCRYPTION STREQUAL OFF)
   message ("Searching for libmongocrypt")
   if (ENABLE_CLIENT_SIDE_ENCRYPTION STREQUAL ON)
      # do not suppress log output if find_package cannot find libmongocrypt
      find_package (mongocrypt)
   elseif (ENABLE_CLIENT_SIDE_ENCRYPTION STREQUAL AUTO)
      find_package (mongocrypt QUIET)
   endif ()

   if (mongocrypt_FOUND)
      set (LIBMONGOCRYPT_TARGET mongo::mongocrypt)
      get_target_property (LIBMONGOCRYPT_RUNTIME_LIBRARY mongo::mongocrypt LOCATION)
      get_filename_component(LIBMONGOCRYPT_LIBRARY_LOCATION ${LIBMONGOCRYPT_RUNTIME_LIBRARY} DIRECTORY)
      find_library (LIBMONGOCRYPT_LIBRARY NAMES mongocrypt PATHS ${LIBMONGOCRYPT_LIBRARY_LOCATION})
      get_target_property (LIBMONGOCRYPT_INCLUDE_DIRECTORIES mongo::mongocrypt INTERFACE_INCLUDE_DIRECTORIES)
      message ("--   libmongocrypt found at ${LIBMONGOCRYPT_LIBRARY}")
      message ("--   libmongocrypt include path ${LIBMONGOCRYPT_INCLUDE_DIRECTORIES}")
      message ("--   libmongocrypt version ${mongocrypt_VERSION}")
      set (MONGOC_ENABLE_CLIENT_SIDE_ENCRYPTION 1)
   elseif (ENABLE_CLIENT_SIDE_ENCRYPTION STREQUAL ON)
      message (FATAL_ERROR "Required library (libmongocrypt) not found.")
   else ()
      message (STATUS "libmongocrypt not found. Configuring without Client-Side Field Level Encryption support.")
   endif ()
endif ()

CHECK_STRUCT_HAS_MEMBER("struct sockaddr_storage" ss_family "sys/socket.h" MONGOC_HAVE_SS_FAMILY)
if (NOT MONGOC_HAVE_SS_FAMILY)
   set (MONGOC_HAVE_SS_FAMILY 0)
else ()
   set (MONGOC_HAVE_SS_FAMILY 1)
endif ()

configure_file (
   "mongo-c-driver/src/libmongoc/src/mongoc/mongoc-config.h.in"
   "${PROJECT_BINARY_DIR}/src/mongoc/mongoc-config.h"
)

configure_file (
   "mongo-c-driver/src/libmongoc/src/mongoc/mongoc-version.h.in"
   "${PROJECT_BINARY_DIR}/src/mongoc/mongoc-version.h"
)

if (ENABLE_APPLE_FRAMEWORK)
   configure_file (
      "mongo-c-driver/src/libmongoc/src/mongoc/modules/module.modulemap.in"
      "${PROJECT_BINARY_DIR}/src/mongoc/modules/module.modulemap"
   )
endif ()

include_directories ("${PROJECT_BINARY_DIR}/src")
include_directories ("${PROJECT_BINARY_DIR}/src/mongoc")
include_directories ("mongo-c-driver/src/libmongoc/src")
add_definitions (-DCOMMON_PREFIX_=_mongoc_common)
include_directories ("mongo-c-driver/src/common")
include_directories ("${PROJECT_BINARY_DIR}/mongo-c-driver/src")
include_directories ("${PROJECT_BINARY_DIR}/mongo-c-driver/src/common")


set (SOURCES ${SOURCES}
   ${libmongoc_path}/src/mongoc/mongoc-aggregate.c
   ${libmongoc_path}/src/mongoc/mongoc-apm.c
   ${libmongoc_path}/src/mongoc/mongoc-array.c
   ${libmongoc_path}/src/mongoc/mongoc-async.c
   ${libmongoc_path}/src/mongoc/mongoc-async-cmd.c
   ${libmongoc_path}/src/mongoc/mongoc-buffer.c
   ${libmongoc_path}/src/mongoc/mongoc-bulk-operation.c
   ${libmongoc_path}/src/mongoc/mongoc-change-stream.c
   ${libmongoc_path}/src/mongoc/mongoc-client.c
   ${libmongoc_path}/src/mongoc/mongoc-client-pool.c
   ${libmongoc_path}/src/mongoc/mongoc-client-side-encryption.c
   ${libmongoc_path}/src/mongoc/mongoc-cluster.c
   ${libmongoc_path}/src/mongoc/mongoc-cluster-aws.c
   ${libmongoc_path}/src/mongoc/mongoc-cluster-sasl.c
   ${libmongoc_path}/src/mongoc/mongoc-collection.c
   ${libmongoc_path}/src/mongoc/mongoc-compression.c
   ${libmongoc_path}/src/mongoc/mongoc-counters.c
   ${libmongoc_path}/src/mongoc/mongoc-crypt.c
   ${libmongoc_path}/src/mongoc/mongoc-cursor-array.c
   ${libmongoc_path}/src/mongoc/mongoc-cursor.c
   ${libmongoc_path}/src/mongoc/mongoc-cursor-cmd.c
   ${libmongoc_path}/src/mongoc/mongoc-cursor-change-stream.c
   ${libmongoc_path}/src/mongoc/mongoc-cursor-cmd-deprecated.c
   ${libmongoc_path}/src/mongoc/mongoc-cursor-find.c
   ${libmongoc_path}/src/mongoc/mongoc-cursor-find-cmd.c
   ${libmongoc_path}/src/mongoc/mongoc-cursor-find-opquery.c
   ${libmongoc_path}/src/mongoc/mongoc-cursor-legacy.c
   ${libmongoc_path}/src/mongoc/mongoc-cursor-array.c
   ${libmongoc_path}/src/mongoc/mongoc-database.c
   ${libmongoc_path}/src/mongoc/mongoc-error.c
   ${libmongoc_path}/src/mongoc/mongoc-find-and-modify.c
   ${libmongoc_path}/src/mongoc/mongoc-init.c
   ${libmongoc_path}/src/mongoc/mongoc-gridfs.c
   ${libmongoc_path}/src/mongoc/mongoc-gridfs-bucket.c
   ${libmongoc_path}/src/mongoc/mongoc-gridfs-bucket-file.c
   ${libmongoc_path}/src/mongoc/mongoc-gridfs-file.c
   ${libmongoc_path}/src/mongoc/mongoc-gridfs-file-list.c
   ${libmongoc_path}/src/mongoc/mongoc-gridfs-file-page.c
   ${libmongoc_path}/src/mongoc/mongoc-gridfs-file-list.c
   ${libmongoc_path}/src/mongoc/mongoc-handshake.c
   ${libmongoc_path}/src/mongoc/mongoc-host-list.c
   ${libmongoc_path}/src/mongoc/mongoc-http.c
   ${libmongoc_path}/src/mongoc/mongoc-index.c
   ${libmongoc_path}/src/mongoc/mongoc-init.c
   ${libmongoc_path}/src/mongoc/mongoc-interrupt.c
   ${libmongoc_path}/src/mongoc/mongoc-list.c
   ${libmongoc_path}/src/mongoc/mongoc-linux-distro-scanner.c
   ${libmongoc_path}/src/mongoc/mongoc-log.c
   ${libmongoc_path}/src/mongoc/mongoc-matcher.c
   ${libmongoc_path}/src/mongoc/mongoc-matcher-op.c
   ${libmongoc_path}/src/mongoc/mongoc-memcmp.c
   ${libmongoc_path}/src/mongoc/mongoc-cmd.c
   ${libmongoc_path}/src/mongoc/mongoc-opts-helpers.c
   ${libmongoc_path}/src/mongoc/mongoc-opts.c
   ${libmongoc_path}/src/mongoc/mongoc-queue.c
   ${libmongoc_path}/src/mongoc/mongoc-read-concern.c
   ${libmongoc_path}/src/mongoc/mongoc-read-prefs.c
   ${libmongoc_path}/src/mongoc/mongoc-rpc.c
   ${libmongoc_path}/src/mongoc/mongoc-server-description.c
   ${libmongoc_path}/src/mongoc/mongoc-server-stream.c
   ${libmongoc_path}/src/mongoc/mongoc-client-session.c
   ${libmongoc_path}/src/mongoc/mongoc-server-monitor.c
   ${libmongoc_path}/src/mongoc/mongoc-set.c
   ${libmongoc_path}/src/mongoc/mongoc-socket.c
   ${libmongoc_path}/src/mongoc/mongoc-stream-buffered.c
   ${libmongoc_path}/src/mongoc/mongoc-stream.c
   ${libmongoc_path}/src/mongoc/mongoc-stream-buffered.c
   ${libmongoc_path}/src/mongoc/mongoc-stream-file.c
   ${libmongoc_path}/src/mongoc/mongoc-stream-gridfs.c
   ${libmongoc_path}/src/mongoc/mongoc-stream-gridfs-download.c
   ${libmongoc_path}/src/mongoc/mongoc-stream-gridfs-upload.c
   ${libmongoc_path}/src/mongoc/mongoc-stream-socket.c
   ${libmongoc_path}/src/mongoc/mongoc-topology.c
   ${libmongoc_path}/src/mongoc/mongoc-topology-background-monitoring.c
   ${libmongoc_path}/src/mongoc/mongoc-topology-description.c
   ${libmongoc_path}/src/mongoc/mongoc-topology-description-apm.c
   ${libmongoc_path}/src/mongoc/mongoc-topology-scanner.c
   ${libmongoc_path}/src/mongoc/mongoc-uri.c
   ${libmongoc_path}/src/mongoc/mongoc-util.c
   ${libmongoc_path}/src/mongoc/mongoc-version-functions.c
   ${libmongoc_path}/src/mongoc/mongoc-write-command.c
   ${libmongoc_path}/src/mongoc/mongoc-write-command-legacy.c
   ${libmongoc_path}/src/mongoc/mongoc-write-concern.c
   ${libmongoc_path}/../../src/common/common-b64.c
   ${libmongoc_path}/../../src/common/common-md5.c
   ${libmongoc_path}/../../src/common/common-thread.c
)

set (HEADERS
   ${PROJECT_BINARY_DIR}/src/mongoc/mongoc-config.h
   ${PROJECT_BINARY_DIR}/src/mongoc/mongoc-version.h
   ${libmongoc_path}/src/mongoc/mongoc.h
   ${libmongoc_path}/src/mongoc/mongoc-apm.h
   ${libmongoc_path}/src/mongoc/mongoc-bulk-operation.h
   ${libmongoc_path}/src/mongoc/mongoc-change-stream.h
   ${libmongoc_path}/src/mongoc/mongoc-client.h
   ${libmongoc_path}/src/mongoc/mongoc-client-pool.h
   ${libmongoc_path}/src/mongoc/mongoc-client-side-encryption.h
   ${libmongoc_path}/src/mongoc/mongoc-collection.h
   ${libmongoc_path}/src/mongoc/mongoc-cursor.h
   ${libmongoc_path}/src/mongoc/mongoc-database.h
   ${libmongoc_path}/src/mongoc/mongoc-error.h
   ${libmongoc_path}/src/mongoc/mongoc-flags.h
   ${libmongoc_path}/src/mongoc/mongoc-find-and-modify.h
   ${libmongoc_path}/src/mongoc/mongoc-gridfs.h
   ${libmongoc_path}/src/mongoc/mongoc-gridfs-bucket.h
   ${libmongoc_path}/src/mongoc/mongoc-gridfs-file.h
   ${libmongoc_path}/src/mongoc/mongoc-gridfs-file-page.h
   ${libmongoc_path}/src/mongoc/mongoc-gridfs-file-list.h
   ${libmongoc_path}/src/mongoc/mongoc-handshake.h
   ${libmongoc_path}/src/mongoc/mongoc-host-list.h
   ${libmongoc_path}/src/mongoc/mongoc-init.h
   ${libmongoc_path}/src/mongoc/mongoc-index.h
   ${libmongoc_path}/src/mongoc/mongoc-iovec.h
   ${libmongoc_path}/src/mongoc/mongoc-log.h
   ${libmongoc_path}/src/mongoc/mongoc-macros.h
   ${libmongoc_path}/src/mongoc/mongoc-matcher.h
   ${libmongoc_path}/src/mongoc/mongoc-opcode.h
   ${libmongoc_path}/src/mongoc/mongoc-prelude.h
   ${libmongoc_path}/src/mongoc/mongoc-read-concern.h
   ${libmongoc_path}/src/mongoc/mongoc-read-prefs.h
   ${libmongoc_path}/src/mongoc/mongoc-server-description.h
   ${libmongoc_path}/src/mongoc/mongoc-client-session.h
   ${libmongoc_path}/src/mongoc/mongoc-socket.h
   ${libmongoc_path}/src/mongoc/mongoc-stream-tls-libressl.h
   ${libmongoc_path}/src/mongoc/mongoc-stream-tls-openssl.h
   ${libmongoc_path}/src/mongoc/mongoc-stream.h
   ${libmongoc_path}/src/mongoc/mongoc-stream-buffered.h
   ${libmongoc_path}/src/mongoc/mongoc-stream-file.h
   ${libmongoc_path}/src/mongoc/mongoc-stream-gridfs.h
   ${libmongoc_path}/src/mongoc/mongoc-stream-socket.h
   ${libmongoc_path}/src/mongoc/mongoc-topology-description.h
   ${libmongoc_path}/src/mongoc/mongoc-uri.h
   ${libmongoc_path}/src/mongoc/mongoc-version-functions.h
   ${libmongoc_path}/src/mongoc/mongoc-write-concern.h
   ${libmongoc_path}/src/mongoc/mongoc-rand.h
   ${libmongoc_path}/src/mongoc/mongoc-stream-tls.h
   ${libmongoc_path}/src/mongoc/mongoc-ssl.h
)

set (HEADERS_FORWARDING
   ${libmongoc_path}/src/mongoc/forwarding/mongoc.h
)

if (NOT ENABLE_SSL STREQUAL OFF)
   set (SOURCES ${SOURCES}
      ${libmongoc_path}/src/mongoc/mongoc-crypto.c
      ${libmongoc_path}/src/mongoc/mongoc-scram.c
      ${libmongoc_path}/src/mongoc/mongoc-stream-tls.c
      ${libmongoc_path}/src/mongoc/mongoc-ssl.c
   )

   if (OPENSSL_FOUND)
      message (STATUS "Compiling against OpenSSL")
      set (SOURCES ${SOURCES}
         ${libmongoc_path}/src/mongoc/mongoc-crypto-openssl.c
         ${libmongoc_path}/src/mongoc/mongoc-rand-openssl.c
         ${libmongoc_path}/src/mongoc/mongoc-stream-tls-openssl.c
         ${libmongoc_path}/src/mongoc/mongoc-stream-tls-openssl-bio.c
         ${libmongoc_path}/src/mongoc/mongoc-openssl.c
         ${libmongoc_path}/src/mongoc/mongoc-ocsp-cache.c
      )
      set (SSL_LIBRARIES ${OPENSSL_LIBRARIES})
      include_directories (${OPENSSL_INCLUDE_DIR})
      if (WIN32)
         set (SSL_LIBRARIES ${SSL_LIBRARIES} crypt32.lib)
      endif ()
   elseif (SECURE_TRANSPORT)
      message (STATUS "Compiling against Secure Transport")
      set (SOURCES ${SOURCES}
         ${libmongoc_path}/src/mongoc/mongoc-crypto-common-crypto.c
         ${libmongoc_path}/src/mongoc/mongoc-rand-common-crypto.c
         ${libmongoc_path}/src/mongoc/mongoc-stream-tls-secure-transport.c
         ${libmongoc_path}/src/mongoc/mongoc-secure-transport.c
      )
      set (SSL_LIBRARIES "-framework CoreFoundation -framework Security")
   elseif (SECURE_CHANNEL)
      message (STATUS "Compiling against Secure Channel")
      set (SOURCES ${SOURCES}
         ${libmongoc_path}/src/mongoc/mongoc-crypto-cng.c
         ${libmongoc_path}/src/mongoc/mongoc-rand-cng.c
         ${libmongoc_path}/src/mongoc/mongoc-stream-tls-secure-channel.c
         ${libmongoc_path}/src/mongoc/mongoc-secure-channel.c
      )
      set (SSL_LIBRARIES secur32.lib crypt32.lib Bcrypt.lib)
   elseif (LIBRESSL)
      message (STATUS "Compiling against LibreSSL")
      set (SOURCES ${SOURCES}
         ${libmongoc_path}/src/mongoc/mongoc-crypto-openssl.c
         ${libmongoc_path}/src/mongoc/mongoc-rand-openssl.c
         ${libmongoc_path}/src/mongoc/mongoc-stream-tls-libressl.c
         ${libmongoc_path}/src/mongoc/mongoc-libressl.c
      )
      set (SSL_LIBRARIES -ltls -lcrypto)
   endif ()
else ()
   message (STATUS "SSL disabled")
endif () # ENABLE_SSL

if (MONGOC_ENABLE_SASL)
   set (SOURCES ${SOURCES} ${libmongoc_path}/src/mongoc/mongoc-sasl.c)
   if (MONGOC_ENABLE_SASL_CYRUS)
      message (STATUS "Compiling against Cyrus SASL")
      set (SOURCES ${SOURCES} ${libmongoc_path}/src/mongoc/mongoc-cluster-cyrus.c)
      set (SOURCES ${SOURCES} ${libmongoc_path}/src/mongoc/mongoc-cyrus.c)
      include_directories (${SASL_INCLUDE_DIRS})
   elseif (MONGOC_ENABLE_SASL_SSPI)
      message (STATUS "Compiling against Windows SSPI")
      set (SOURCES ${SOURCES} ${libmongoc_path}/src/mongoc/mongoc-cluster-sspi.c)
      set (SOURCES ${SOURCES} ${libmongoc_path}/src/mongoc/mongoc-sspi.c)
      set (SASL_LIBRARIES secur32.lib crypt32.lib Shlwapi.lib)
   endif ()
else ()
   message (STATUS "SASL disabled")
endif ()


set (THREADS_PREFER_PTHREAD_FLAG 1)
find_package (Threads REQUIRED)
if (CMAKE_USE_PTHREADS_INIT)
   set (THREAD_LIB ${CMAKE_THREAD_LIBS_INIT})
endif ()

set (LIBRARIES
   ${SASL_LIBRARIES} ${SSL_LIBRARIES} ${SHM_LIBRARIES} ${RESOLV_LIBRARIES}
   snappy ${ZLIB_LIBRARIES} ${MONGOC_ZSTD_LIBRARIES} Threads::Threads ${ICU_LIBRARIES} ${LIBMONGOCRYPT_LIBRARY}
)
set (STATIC_LIBRARIES
   ${SASL_LIBRARIES} ${SSL_LIBRARIES} ${SHM_LIBRARIES} ${RESOLV_LIBRARIES}
   snappy ${ZLIB_LIBRARIES} ${MONGOC_ZSTD_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${ICU_LIBRARIES} ${LIBMONGOCRYPT_LIBRARY}
)

if (WIN32)
   set (LIBRARIES ${LIBRARIES} ws2_32)
endif ()

if (MONGOC_ENABLE_MONGODB_AWS_AUTH)
   # Disable warnings on bundled kms_message source files.
   set_source_files_properties (${KMS_MSG_SOURCES} PROPERTIES COMPILE_FLAGS -w)
   set (SOURCES ${SOURCES} ${KMS_MSG_SOURCES})

   set (KMS_MSG_DEFINITIONS KMS_MSG_STATIC KMS_MESSAGE_ENABLE_CRYPTO)

   if (MONGOC_ENABLE_CRYPTO_CNG)
      set (KMS_MSG_DEFINITIONS ${KMS_MSG_DEFINITIONS} KMS_MESSAGE_ENABLE_CRYPTO_CNG)
   elseif (MONGOC_ENABLE_CRYPTO_COMMON_CRYPTO)
      set (KMS_MSG_DEFINITIONS ${KMS_MSG_DEFINITIONS} KMS_MESSAGE_ENABLE_CRYPTO_COMMON_CRYPTO)
   elseif(MONGOC_ENABLE_CRYPTO_LIBCRYPTO)
      set (KMS_MSG_DEFINITIONS ${KMS_MSG_DEFINITIONS} KMS_MESSAGE_ENABLE_CRYPTO_LIBCRYPTO)
   else ()
      message (FATAL_ERROR "MONGODB-AWS requires a crypto library")
   endif()
endif ()

add_library (mongoc_shared SHARED ${SOURCES} ${HEADERS} ${HEADERS_FORWARDING})
set_target_properties (mongoc_shared PROPERTIES CMAKE_CXX_VISIBILITY_PRESET hidden)
target_link_libraries (mongoc_shared PRIVATE ${LIBRARIES} PUBLIC ${BSON_LIBRARIES})
target_include_directories (mongoc_shared BEFORE PUBLIC ${MONGOC_INTERNAL_INCLUDE_DIRS})
target_include_directories (mongoc_shared PRIVATE ${PRIVATE_ZLIB_INCLUDES})
target_include_directories (mongoc_shared PRIVATE ${LIBMONGOCRYPT_INCLUDE_DIRECTORIES})
if (MONGOC_ENABLE_MONGODB_AWS_AUTH)
   target_include_directories (mongoc_shared PRIVATE "${libmongoc_path}/../kms-message/src")
   if (APPLE)
      set_target_properties (mongoc_shared PROPERTIES LINK_FLAGS "-Wl,-unexported_symbols_list,${libmongoc_path}/../../build/cmake/libmongoc-hidden-symbols.txt")
   elseif (UNIX)
      set_target_properties (mongoc_shared PROPERTIES LINK_FLAGS "-Wl,--version-script=${libmongoc_path}/../../build/cmake/libmongoc-hidden-symbols.map")
   endif ()

endif ()
target_compile_definitions (mongoc_shared PRIVATE MONGOC_COMPILATION ${KMS_MSG_DEFINITIONS})

set_target_properties (mongoc_shared PROPERTIES VERSION 0.0.0 SOVERSION 0)
set_target_properties (mongoc_shared PROPERTIES OUTPUT_NAME "${MONGOC_OUTPUT_BASENAME}-${MONGOC_API_VERSION}")

if (MONGOC_ENABLE_STATIC_BUILD)
   add_library (mongoc_static STATIC ${SOURCES} ${HEADERS} ${HEADERS_FORWARDING})
   target_link_libraries (mongoc_static ${STATIC_LIBRARIES} ${BSON_STATIC_LIBRARIES})
   if (NOT WIN32 AND ENABLE_PIC)
      target_compile_options (mongoc_static PUBLIC -fPIC)
      message ("Adding -fPIC to compilation of mongoc_static components")
   endif ()
   target_include_directories (mongoc_static BEFORE PUBLIC ${MONGOC_INTERNAL_INCLUDE_DIRS})
   target_include_directories (mongoc_static PRIVATE ${PRIVATE_ZLIB_INCLUDES})
   target_include_directories (mongoc_static PRIVATE ${LIBMONGOCRYPT_INCLUDE_DIRECTORIES})
   if (MONGOC_ENABLE_MONGODB_AWS_AUTH)
      target_include_directories (mongoc_static PRIVATE "${libmongoc_path}/../kms-message/src")
   endif ()
   target_compile_definitions (mongoc_static
      PUBLIC MONGOC_STATIC ${BSON_STATIC_PUBLIC_DEFINITIONS}
      PRIVATE MONGOC_COMPILATION ${KMS_MSG_DEFINITIONS}
   )
   set_target_properties (mongoc_static PROPERTIES VERSION 0.0.0)
   set_target_properties (mongoc_static PROPERTIES OUTPUT_NAME "${MONGOC_OUTPUT_BASENAME}-static-${MONGOC_API_VERSION}")
endif ()

if (ENABLE_APPLE_FRAMEWORK)
   set_target_properties (mongoc_shared PROPERTIES
      FRAMEWORK TRUE
      MACOSX_FRAMEWORK_BUNDLE_VERSION ${MONGOC_VERSION}
      MACOSX_FRAMEWORK_SHORT_VERSION_STRING ${MONGOC_VERSION}
      MACOSX_FRAMEWORK_IDENTIFIER org.mongodb.mongoc
      OUTPUT_NAME "${MONGOC_OUTPUT_BASENAME}"
      PUBLIC_HEADER "${HEADERS}"
   )
endif ()

# mongoc-stat works if shared memory performance counters are enabled.
if (ENABLE_SHM_COUNTERS STREQUAL "ON")
   install (PROGRAMS ${PROJECT_BINARY_DIR}/mongoc-stat
      DESTINATION ${CMAKE_INSTALL_BINDIR}
   )
endif ()


if (MONGOC_ENABLE_STATIC_INSTALL)
   set (TARGETS_TO_INSTALL mongoc_shared mongoc_static)
else ()
   set (TARGETS_TO_INSTALL mongoc_shared)
endif ()

set (MONGOC_HEADER_INSTALL_DIR
   "${CMAKE_INSTALL_INCLUDEDIR}/libmongoc-${MONGOC_API_VERSION}"
)

install (
   TARGETS ${TARGETS_TO_INSTALL} ${EXAMPLES}
   EXPORT mongoc-targets
   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
   ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
   INCLUDES DESTINATION ${MONGOC_HEADER_INSTALL_DIR}
   FRAMEWORK DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install (
   FILES ${HEADERS}
   DESTINATION "${MONGOC_HEADER_INSTALL_DIR}/mongoc"
)

install (
   FILES ${HEADERS_FORWARDING}
   DESTINATION "${MONGOC_HEADER_INSTALL_DIR}"
)

if (ENABLE_APPLE_FRAMEWORK)
   install (
      FILES "${PROJECT_BINARY_DIR}/src/mongoc/modules/module.modulemap"
      DESTINATION "${CMAKE_INSTALL_BINDIR}/mongoc.framework/Modules/"
   )
endif ()

# Define pkg-config files
set (VERSION "${MONGOC_VERSION}")
set (prefix "${CMAKE_INSTALL_PREFIX}")
set (libdir "\${prefix}/${CMAKE_INSTALL_LIBDIR}")

foreach (
      FLAG
      ${SASL_LIBRARIES} ${SSL_LIBRARIES} ${SHM_LIBRARIES} ${RESOLV_LIBRARIES}
      ${THREAD_LIB} ${ZLIB_LIBRARIES} ${SNAPPY_LIBRARIES} ${MONGOC_ZSTD_LIBRARIES} ${ICU_LIBRARIES}
      ${LIBMONGOCRYPT_LIBRARY})

   if (IS_ABSOLUTE "${FLAG}")
      get_filename_component (FLAG_DIR "${FLAG}" DIRECTORY)
      get_filename_component (FLAG_FILE "${FLAG}" NAME_WE)
      STRING (REGEX REPLACE "^lib" "" FLAG_FILE "${FLAG_FILE}")
      set (MONGOC_LIBRARIES "${MONGOC_LIBRARIES} -L${FLAG_DIR} -l${FLAG_FILE}")
   elseif (FLAG MATCHES "^-.*")
      # Flag starts with dash, add it as-is.
      set (MONGOC_LIBRARIES "${MONGOC_LIBRARIES} ${FLAG}")
   else ()
      # Flag doesn't start with dash, add it with a dash.
      set (MONGOC_LIBRARIES "${MONGOC_LIBRARIES} -l${FLAG}")
   endif ()
endforeach ()

configure_file (
   ${libmongoc_path}/src/libmongoc-1.0.pc.in
   ${CMAKE_CURRENT_BINARY_DIR}/src/libmongoc-1.0.pc
@ONLY)
install (
   FILES ${CMAKE_CURRENT_BINARY_DIR}/src/libmongoc-1.0.pc
   DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
if (MONGOC_ENABLE_STATIC_INSTALL)
   configure_file (
      ${libmongoc_path}/src/libmongoc-static-1.0.pc.in
      ${CMAKE_CURRENT_BINARY_DIR}/src/libmongoc-static-1.0.pc
   @ONLY)
   install (
      FILES ${CMAKE_CURRENT_BINARY_DIR}/src/libmongoc-static-1.0.pc
      DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
   )
endif ()
# Deprecated alias for libmongoc-1.0.pc, see CDRIVER-2086.
if (MONGOC_ENABLE_SSL)
   configure_file (
      ${libmongoc_path}/src/libmongoc-ssl-1.0.pc.in
      ${CMAKE_CURRENT_BINARY_DIR}/src/libmongoc-ssl-1.0.pc
   @ONLY)
   install (
      FILES ${CMAKE_CURRENT_BINARY_DIR}/src/libmongoc-ssl-1.0.pc
      DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
   )
endif ()

include (CMakePackageConfigHelpers)
set (INCLUDE_INSTALL_DIRS "${MONGOC_HEADER_INSTALL_DIR}")
set (LIBRARY_INSTALL_DIRS ${CMAKE_INSTALL_LIBDIR})

write_basic_package_version_file (
   "${CMAKE_CURRENT_BINARY_DIR}/mongoc/mongoc-${MONGOC_API_VERSION}-config-version.cmake"
   VERSION ${MONGOC_VERSION}
   COMPATIBILITY AnyNewerVersion
)

export (EXPORT mongoc-targets
   NAMESPACE mongo::
   FILE "${CMAKE_CURRENT_BINARY_DIR}/mongoc/mongoc-targets.cmake"
)

configure_file (${libmongoc_path}/src/mongoc-config.cmake
   "${CMAKE_CURRENT_BINARY_DIR}/mongoc/mongoc-${MONGOC_API_VERSION}-config.cmake"
   @ONLY
)

set (INCLUDE_INSTALL_DIRS ${MONGOC_HEADER_INSTALL_DIR})
set (LIBRARY_INSTALL_DIRS ${CMAKE_INSTALL_LIBDIR})
set (PACKAGE_LIBRARIES mongoc-1.0)

include (CMakePackageConfigHelpers)

# These aren't pkg-config files, they're CMake package configuration files.
function (install_package_config_file prefix)
   foreach (suffix "config.cmake" "config-version.cmake")
      configure_package_config_file (
         ${libmongoc_path}/build/cmake/libmongoc-${prefix}-${suffix}.in
         ${CMAKE_CURRENT_BINARY_DIR}/libmongoc-${prefix}-${suffix}
         INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libmongoc-${prefix}
         PATH_VARS INCLUDE_INSTALL_DIRS LIBRARY_INSTALL_DIRS
      )

      install (
         FILES
            ${CMAKE_CURRENT_BINARY_DIR}/libmongoc-${prefix}-${suffix}
         DESTINATION
            ${CMAKE_INSTALL_LIBDIR}/cmake/libmongoc-${prefix}
      )
   endforeach ()
endfunction ()

install_package_config_file ("1.0")

if (ENABLE_STATIC)
   install_package_config_file ("static-1.0")
endif ()

# sub-directory 'doc' was already included above
add_subdirectory (${libmongoc_path}/src/mongoc)

set_local_dist (src_libmongoc_DIST_local
   CMakeLists.txt
   THIRD_PARTY_NOTICES
)

set (src_libmongoc_DIST
   ${src_libmongoc_DIST_local}
   ${src_libmongoc_build_DIST}
   ${src_libmongoc_doc_DIST}
   ${src_libmongoc_examples_DIST}
   ${src_libmongoc_src_DIST}
   ${src_libmongoc_tests_DIST}
   PARENT_SCOPE
)
