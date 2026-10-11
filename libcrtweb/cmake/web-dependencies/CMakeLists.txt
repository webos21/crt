# Does WebKit's own dependency detection accept the CRT ports? (Web Tranche 3B, milestone B2.)
#
# Runs the find_package() calls of the Win and PlayStation ports (Source/cmake/OptionsWin.cmake,
# OptionsPlayStation.cmake: the non-GLib ports PlatformCRT is modelled on) with WebKit's own Find
# modules, against a CRT port prefix, and prints what each resolved to. Driven by
# tools/check_web_dependencies.py, which supplies the toolchain file and the pkg-config
# environment. Nothing is built.
cmake_minimum_required(VERSION 3.25)
project(crtweb_dependencies C CXX)

if(NOT WEBKIT_SOURCE_DIR)
  message(FATAL_ERROR "set -DWEBKIT_SOURCE_DIR=<product source>")
endif()
list(APPEND CMAKE_MODULE_PATH "${WEBKIT_SOURCE_DIR}/Source/cmake")

set(CRT_DEPENDENCY_REPORT "")

function(crt_expect name)
  if(${name}_FOUND OR ${name}_FOUND)
    message(STATUS "RESULT ${name}: found")
  else()
    message(FATAL_ERROR "RESULT ${name}: NOT FOUND")
  endif()
endfunction()

find_package(CURL 7.87.0 REQUIRED)
find_package(HarfBuzz 1.4.2 REQUIRED COMPONENTS ICU)
find_package(ICU 70.1 REQUIRED COMPONENTS data i18n uc)
find_package(JPEG 1.5.2 REQUIRED)
find_package(LibXml2 2.9.7 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PNG 1.6.34 REQUIRED)
find_package(SQLite3 3.23.1 REQUIRED)
find_package(ZLIB 1.2.11 REQUIRED)
find_package(LibPSL 0.20.2 REQUIRED)
find_package(WebP REQUIRED COMPONENTS demux)

message(STATUS "RESULT CURL ${CURL_VERSION_STRING} ${CURL_LIBRARIES}")
message(STATUS "RESULT HarfBuzz ${HarfBuzz_VERSION} ${HarfBuzz_LIBRARY}")
message(STATUS "RESULT ICU ${ICU_VERSION} ${ICU_UC_LIBRARY}")
message(STATUS "RESULT JPEG ${JPEG_VERSION} ${JPEG_LIBRARIES}")
message(STATUS "RESULT LibXml2 ${LIBXML2_VERSION_STRING} ${LIBXML2_LIBRARIES}")
message(STATUS "RESULT OpenSSL ${OPENSSL_VERSION} ${OPENSSL_SSL_LIBRARY}")
message(STATUS "RESULT PNG ${PNG_VERSION_STRING} ${PNG_LIBRARIES}")
message(STATUS "RESULT SQLite3 ${SQLite3_VERSION} ${SQLite3_LIBRARIES}")
message(STATUS "RESULT ZLIB ${ZLIB_VERSION_STRING} ${ZLIB_LIBRARIES}")
message(STATUS "RESULT LibPSL ${LibPSL_VERSION} ${LibPSL_LIBRARIES}")
message(STATUS "RESULT WebP ${WebP_VERSION} ${WebP_LIBRARIES}")
