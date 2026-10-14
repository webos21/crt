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

# WebCore for the CRT platform (Web Tranche 3B, milestone B3). Modelled on PlatformPlayStation.cmake:
# curl + OpenSSL networking, the generic scroll animation and keyed coders, Skia painting. Differences:
#   * libwpe is the static port (porting/recipes/libwpe.json): key codes and the pasteboard come
#     from it as in the PlayStation port, the embedder defines the loader interface (B4);
#   * no EGL/GL context is ever created: painting is CPU-only until Tranche 9, and the EGL/GLES
#     libraries are the CRT "no GPU" boundary (porting/recipes/crt-egl.json).
# Files under */playstation/ that are plain stubs are used from the pinned upstream tree unchanged.

include(platform/Curl.cmake)
include(platform/ImageDecoders.cmake)
include(platform/OpenSSL.cmake)
include(platform/Skia.cmake)
include(platform/TextureMapper.cmake)

list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    ${WEBCORE_DIR}/platform
    ${WEBCORE_DIR}/platform/generic
    ${WEBCORE_DIR}/platform/mediacapabilities
    ${WEBCORE_DIR}/platform/video-codecs
    ${WEBCORE_DIR}/platform/graphics/egl
    ${WEBCORE_DIR}/platform/graphics/opengl
    ${WEBCORE_DIR}/platform/crt
    ${WEBCORE_DIR}/rendering/playstation
)

list(APPEND WebCore_SOURCES
    accessibility/playstation/AXObjectCachePlayStation.cpp
    accessibility/playstation/AccessibilityObjectPlayStation.cpp

    page/playstation/ResourceUsageOverlayPlayStation.cpp
    page/playstation/ResourceUsageThreadPlayStation.cpp

    platform/ScrollAnimationKinetic.cpp
    platform/ScrollAnimationSmooth.cpp

    editing/libwpe/EditorLibWPE.cpp

    platform/generic/KeyedDecoderGeneric.cpp
    platform/generic/KeyedEncoderGeneric.cpp

    platform/graphics/DMABufBuffer.cpp
    platform/graphics/PlatformDisplay.cpp


    platform/libwpe/PasteboardLibWPE.cpp
    platform/libwpe/PlatformKeyboardEventLibWPE.cpp
    platform/libwpe/PlatformPasteboardLibWPE.cpp

    platform/graphics/egl/GLContext.cpp
    platform/graphics/egl/GLContextWrapper.cpp
    platform/graphics/egl/GLDisplay.cpp
    platform/graphics/egl/GLFence.cpp
    platform/graphics/egl/GLFenceEGL.cpp
    platform/graphics/egl/GLFenceGL.cpp

    platform/crt/SystemFontDatabaseCRT.cpp
    platform/crt/UserAgentCRT.cpp
    platform/network/crt/CurlSSLHandleCRT.cpp
    platform/network/playstation/NetworkStateNotifierPlayStation.cpp

    platform/playstation/MIMETypeRegistryPlayStation.cpp
    platform/playstation/PlatformScreenPlayStation.cpp
    platform/playstation/ScrollbarThemePlayStation.cpp
    platform/playstation/ThemePlayStation.cpp

    platform/text/Hyphenation.cpp
    platform/text/LocaleICU.cpp

    platform/unix/LoggingUnix.cpp
    platform/unix/SharedMemoryUnix.cpp

    rendering/playstation/RenderThemePlayStation.cpp
)

# WebKit's CMake list of the bundled Skia (Source/ThirdParty/skia/CMakeLists.txt) lacks
# src/core/SkStrikeRef.cpp, which Skia's own gn list has and SkFont::makeStrikeRef() needs. ELF links
# drop the unreferenced method with --gc-sections; a COFF link reports the undefined symbol.
if (TARGET Skia)
    target_sources(Skia PRIVATE "${THIRDPARTY_DIR}/skia/src/core/SkStrikeRef.cpp")
endif ()

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    platform/graphics/DMABufBuffer.h
    platform/graphics/DMABufBufferAttributes.h

    rendering/playstation/RenderThemePlayStation.h
)

list(APPEND WebCore_LIBRARIES
    WPE::libwpe
    Threads::Threads
)

# Tranche 8B: the playback engine over the installed crtmedia library (public C API only).
if (CRT_ENABLE_CRTMEDIA_PLAYER)
    list(APPEND WebCore_SOURCES
        platform/graphics/crt/MediaPlayerPrivateCRT.cpp
    )
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/graphics/crt"
    )
    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        platform/graphics/crt/MediaPlayerPrivateCRT.h
    )
    # Tranche 8C: the CRT video capture source (compiled when MediaStream is enabled).
    if (ENABLE_MEDIA_STREAM)
        list(APPEND WebCore_SOURCES
            platform/mediastream/crt/CRTVideoCaptureSource.cpp
            platform/mediastream/crt/MockRealtimeSourcesCRT.cpp
        )
        # The generic branch of the mock capture sources does not build (see MockRealtimeSourcesCRT.cpp).
        list(APPEND WebCore_UNIFIED_SOURCE_EXCLUDES
            "platform/mock/MockRealtimeAudioSource.cpp"
            "platform/mock/MockRealtimeVideoSource.cpp"
        )
        list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
            "${WEBCORE_DIR}/platform/mediastream/crt"
        )
        list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
            platform/mediastream/crt/CRTVideoCaptureSource.h
        )
    endif ()
    list(APPEND WebCore_LIBRARIES ${CRT_CRTMEDIA_LIBRARY})
endif ()

# WebKit's OpenSSL crypto sources are written against OpenSSL 1.1/BoringSSL, where
# EVP_PKEY_get0_RSA() returns a mutable RSA*; OpenSSL 3 (the CRT port) returns const RSA*.
# A force-included header restores the old signature for that file only (no source patch).
set_source_files_properties(crypto/openssl/CryptoKeyRSAOpenSSL.cpp PROPERTIES
    COMPILE_OPTIONS "-include;${CMAKE_CURRENT_LIST_DIR}/platform/crt/OpenSSL3Compat.h")

# WebCore's GL code uses ES 3 formats (GL_RGBA8, GL_R8, ...) through <GLES2/gl2.h> alone, which the
# Mesa/Khronos system headers provide as a side effect and WebKit's own ANGLE copy does not.
# GL_R16/GL_RG16 are EXT_texture_norm16's GL_R16_EXT (0x822A) and GL_RG16_EXT (0x822C).
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -include GLES3/gl3.h -DGL_R16=0x822A -DGL_RG16=0x822C")

# The B3 gate program (libcrtweb/tests/webcore): WTF + JavaScriptCore + PAL + WebCore + Skia in one
# executable, built only when the driver passes -DCRT_WEBCORE_TESTS=<dir>.
if (CRT_WEBCORE_TESTS)
    add_executable(crtweb_webcore_graph_test ${CRT_WEBCORE_TESTS}/crtweb_webcore_graph_test.cpp)
    target_include_directories(crtweb_webcore_graph_test PRIVATE
        $<TARGET_PROPERTY:WebCore,INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:JavaScriptCore,INCLUDE_DIRECTORIES>
        ${WebCore_FRAMEWORK_HEADERS_DIR}
        ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
    )
    target_link_libraries(crtweb_webcore_graph_test PRIVATE WebCore PAL JavaScriptCore WTF bmalloc Skia
        ${WebCore_LIBRARIES})
endif ()
