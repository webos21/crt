# Copyright (c) 2026 The CRT Project. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
# OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
# OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

# The PlatformCRT port (Web Tranche 3B), selected with -DPORT=CRT; the name is registered by
# libcrtweb/patches 0005. The port has two configurations, chosen by CRT_ENABLE_WEBCORE:
#
#   OFF (default)  milestone B1: JavaScriptCore exactly as JSCOnly builds it.
#   ON             milestone B3: WTF, JavaScriptCore, the bundled Skia, PAL and WebCore for the CRT
#                  platform. With CRT_ENABLE_WEBKIT=ON as well (milestone B4): WebKit, its Web and
#                  Network process executables and the CRT process launcher.
#
# The ON configuration is modelled on OptionsPlayStation.cmake (a non-GLib, non-Apple port with a
# generic run loop, curl, OpenSSL and a C embedding API), with Skia in place of Cairo as upstream
# 2.54 requires and with no libwpe, no fontconfig use and no GPU path (docs/acceptance/
# crtweb_acceptance.md, "3B plan", decisions D2-D8).

option(CRT_ENABLE_WEBCORE "Build WebCore and PAL for the CRT platform" OFF)
option(CRT_ENABLE_WEBKIT "Also build WebKit (UI, Web and Network process code); needs CRT_ENABLE_WEBCORE" OFF)

if (NOT CRT_ENABLE_WEBCORE)
    include(OptionsJSCOnly)
else ()
    set(PORT CRT)

    # Dependencies are CRT ports installed under one prefix (porting/recipes, group "web").
    # Clang deduplicates #pragma once per file, so the framework header copies WebKit makes in the
    # build tree must be symlinks (patch 0012). A Windows build host can create them only in Developer
    # Mode (or elevated), so it is probed once: without symlinks the copies remain and the clang
    # redefinition errors of B4 follow, which the probe's message names.
    if (NOT CMAKE_HOST_WIN32)
        set(CRT_SYMLINK_FRAMEWORK_HEADERS ON)
    else ()
        set(_crt_link_probe "${CMAKE_BINARY_DIR}/crt-symlink-probe")
        file(REMOVE "${_crt_link_probe}")
        file(CREATE_LINK "${CMAKE_CURRENT_LIST_FILE}" "${_crt_link_probe}" SYMBOLIC RESULT _crt_link_result)
        file(REMOVE "${_crt_link_probe}")
        if (_crt_link_result STREQUAL "0")
            set(CRT_SYMLINK_FRAMEWORK_HEADERS ON)
        else ()
            message(WARNING "CRT: this Windows host cannot create symbolic links (${_crt_link_result}); "
                "enable Developer Mode: WebKit's framework header copies need to be links for clang")
        endif ()
    endif ()

    # A PE DLL has at most 65,535 exports and lld's MinGW driver exports everything that is not marked
    # otherwise: libWebKit has ~207,000 symbols. Every consumer is a CRT program that links WebKit
    # whole (the Web and Network processes, the UI-process test), so it is a static archive there.
    if (CMAKE_HOST_WIN32)
        set(WebKit_LIBRARY_TYPE STATIC)
    endif ()

    find_package(Threads REQUIRED)
    find_package(ICU 70.1 REQUIRED COMPONENTS data i18n uc)

    set(ENABLE_WEBKIT_LEGACY OFF)
    set(ENABLE_WEBINSPECTORUI OFF)
    if (CRT_ENABLE_WEBKIT)
        set(ENABLE_WEBKIT ON)
    else ()
        set(ENABLE_WEBKIT OFF)
    endif ()

    # EGL and GLES2: WebCore links them unconditionally and Skia's Ganesh GL sources always
    # compile. Headers are WebKit's own copy (ThirdParty/ANGLE/include); the libraries are the
    # CRT "no GPU" boundary (porting/recipes/crt-egl.json, libcrtweb/platform/gles-stub) in the port prefix.
    include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/Source/ThirdParty/ANGLE/include)

    # libwpe is a static library here (the PlayStation shape): the embedder -- PlatformCRT's backend
    # -- defines _wpe_loader_interface. It is installed as libwpe.a, not wpe-1.0.
    set(WPE_NAMES wpe)
    find_package(WPE 1.14.0 REQUIRED)

    find_package(EGL REQUIRED)
    find_package(OpenGLES2 REQUIRED)
    find_package(Fontconfig 2.13.0 REQUIRED)
    find_package(CURL 7.85.0 REQUIRED)
    find_package(Freetype REQUIRED)
    find_package(HarfBuzz REQUIRED COMPONENTS ICU)
    find_package(JPEG 1.5.2 REQUIRED)
    find_package(LibPSL 0.20.2 REQUIRED)
    find_package(LibXml2 2.9.7 REQUIRED)
    find_package(OpenSSL REQUIRED)
    find_package(PNG REQUIRED)
    find_package(SQLite3 3.23.1 REQUIRED)
    if (NOT TARGET SQLite3::SQLite3) # CMake < 4.3 names it SQLite::SQLite3
        add_library(SQLite3::SQLite3 ALIAS SQLite::SQLite3)
    endif ()
    find_package(WebP REQUIRED COMPONENTS demux)
    find_package(ZLIB 1.2.11 REQUIRED)

    # The CRT playback engine (Tranche 8B) consumes the installed crtmedia public C API. It is Linux
    # only until its host replays, and needs the driver's libcrtweb directory for the VA-API stub.
    if (NOT WIN32 AND NOT APPLE AND CRT_LIBCRTWEB_DIR)
        find_library(CRT_CRTMEDIA_LIBRARY NAMES crtmedia PATHS "${CMAKE_SYSROOT}/lib" NO_DEFAULT_PATH)
        if (CRT_CRTMEDIA_LIBRARY)
            set(CRT_ENABLE_CRTMEDIA_PLAYER ON)
            message(STATUS "CRT playback engine: ${CRT_CRTMEDIA_LIBRARY}")
        endif ()
    endif ()

    WEBKIT_OPTION_BEGIN()

    WEBKIT_OPTION_DEFINE(ENABLE_STATIC_JSC "Control whether to build a non-shared JSC" PUBLIC ON)

    WEBKIT_OPTION_DEFAULT_PORT_VALUE(USE_ISO_MALLOC PRIVATE OFF)

    # CPU painting only until Tranche 9 (GPU buffers). HTML video is compiled, but the crtmedia
    # playback engine is a separate Tranche 8B gate; no WebRTC, WebGL or WebGPU.
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(USE_SKIA PRIVATE ON)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(USE_SKIA_ENCODERS PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_GPU_PROCESS PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBGL PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEB_AUDIO PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_ASYNC_SCROLLING PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_CONTEXT_MENUS PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_GEOLOCATION PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_IMAGE_DIFF PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MATHML PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_NOTIFICATIONS PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_USER_MESSAGE_HANDLERS PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_XSLT PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_REMOTE_INSPECTOR PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(USE_WOFF2 PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(USE_AVIF PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(USE_LCMS PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(USE_JPEGXL PRIVATE OFF)

    WEBKIT_OPTION_END()

    SET_AND_EXPOSE_TO_BUILD(USE_LIBWPE ON)
    SET_AND_EXPOSE_TO_BUILD(USE_CURL ON)
    SET_AND_EXPOSE_TO_BUILD(USE_HARFBUZZ ON)
    SET_AND_EXPOSE_TO_BUILD(USE_OPENSSL ON)
    SET_AND_EXPOSE_TO_BUILD(USE_UNIX_DOMAIN_SOCKETS ON)
    SET_AND_EXPOSE_TO_BUILD(USE_TEXTURE_MAPPER ON)
    SET_AND_EXPOSE_TO_BUILD(USE_COORDINATED_GRAPHICS ON)
    SET_AND_EXPOSE_TO_BUILD(ENABLE_SCROLLING_THREAD OFF)
endif ()

add_definitions(-DBUILDING_CRT__=1 -DWTF_PLATFORM_CRT=1)
