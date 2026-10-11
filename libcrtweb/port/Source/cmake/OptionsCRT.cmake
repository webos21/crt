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
#                  platform. WebKit (UI/Web/Network process, milestone B4) is not enabled yet.
#
# The ON configuration is modelled on OptionsPlayStation.cmake (a non-GLib, non-Apple port with a
# generic run loop, curl, OpenSSL and a C embedding API), with Skia in place of Cairo as upstream
# 2.54 requires and with no libwpe, no fontconfig use and no GPU path (docs/acceptance/
# crtweb_acceptance.md, "3B plan", decisions D2-D8).

option(CRT_ENABLE_WEBCORE "Build WebCore and PAL for the CRT platform" OFF)

if (NOT CRT_ENABLE_WEBCORE)
    include(OptionsJSCOnly)
else ()
    set(PORT CRT)

    # Dependencies are CRT ports installed under one prefix (porting/recipes, group "web").
    find_package(Threads REQUIRED)
    find_package(ICU 70.1 REQUIRED COMPONENTS data i18n uc)

    set(ENABLE_WEBKIT_LEGACY OFF)
    set(ENABLE_WEBINSPECTORUI OFF)
    set(ENABLE_WEBKIT OFF)

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

    WEBKIT_OPTION_BEGIN()

    WEBKIT_OPTION_DEFINE(ENABLE_STATIC_JSC "Control whether to build a non-shared JSC" PUBLIC ON)

    WEBKIT_OPTION_DEFAULT_PORT_VALUE(USE_ISO_MALLOC PRIVATE OFF)

    # CPU painting only until Tranche 9 (GPU buffers); no media, WebRTC, WebGL or WebGPU.
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
