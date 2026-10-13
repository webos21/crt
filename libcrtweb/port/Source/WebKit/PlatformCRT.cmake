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

# WebKit for the CRT platform (Web Tranche 3B, milestone B4). Modelled on
# PlatformPlayStation.cmake: generic Unix IPC (SOCK_SEQPACKET + SCM_RIGHTS), the libwpe event and
# pasteboard files, CoordinatedGraphics, the WKView C API. Differences:
#   * the process launcher is CRT's (UIProcess/Launcher/crt), posix_spawn on the Unix hosts;
#   * the Web and Network process executables use the upstream Unix entry points and link WebKit
#     statically, instead of dlopen()ing a list of libraries (CRT's libdl has no ELF loader on
#     Linux, and the executables are ordinary programs);
#   * no GPU process, WebGL, media or WebRTC (OptionsCRT.cmake).
include(Headers.cmake)
include(Platform/Curl.cmake)

WEBKIT_ADD_TARGET_CXX_FLAGS(WebKit -Wno-unused-lambda-capture)

list(APPEND WebProcess_SOURCES
    WebProcess/EntryPoint/unix/WebProcessMain.cpp
)
list(APPEND WebProcess_INCLUDE_DIRECTORIES ${WebKit_PRIVATE_INCLUDE_DIRECTORIES})
list(APPEND WebProcess_PRIVATE_LIBRARIES
    ${EGL_LIBRARIES}
    OpenSSL::Crypto
)

list(APPEND NetworkProcess_SOURCES
    NetworkProcess/EntryPoint/unix/NetworkProcessMain.cpp
)
list(APPEND NetworkProcess_INCLUDE_DIRECTORIES ${WebKit_PRIVATE_INCLUDE_DIRECTORIES})
list(APPEND NetworkProcess_PRIVATE_LIBRARIES
    OpenSSL::Crypto
)

list(APPEND WebKit_SOURCES
    NetworkProcess/NetworkDataTaskDataURL.cpp

    NetworkProcess/Classifier/WebResourceLoadStatisticsStore.cpp

    Platform/IPC/unix/ArgumentCodersUnix.cpp
    Platform/IPC/unix/ConnectionUnix.cpp
    Platform/IPC/unix/IPCSemaphoreUnix.cpp
    Platform/IPC/unix/IPCUtilitiesUnix.cpp

    Platform/classifier/ResourceLoadStatisticsClassifier.cpp

    Platform/crt/WPELoaderCRT.cpp

    Platform/unix/LoggingUnix.cpp
    Platform/unix/ModuleUnix.cpp

    Shared/API/c/playstation/WKEventPlayStation.cpp

    Shared/libwpe/NativeWebKeyboardEventLibWPE.cpp
    Shared/libwpe/NativeWebMouseEventLibWPE.cpp
    Shared/libwpe/NativeWebTouchEventLibWPE.cpp
    Shared/libwpe/NativeWebWheelEventLibWPE.cpp
    Shared/libwpe/WebEventFactory.cpp

    Shared/unix/AuxiliaryProcessMain.cpp

    UIProcess/DefaultUndoController.cpp
    UIProcess/LegacySessionStateCodingNone.cpp
    UIProcess/WebGrammarDetail.cpp
    UIProcess/WebMemoryPressureHandler.cpp
    UIProcess/WebViewportAttributes.cpp

    UIProcess/API/C/WKUserScriptRef.cpp
    UIProcess/API/C/WKViewportAttributes.cpp

    UIProcess/API/C/playstation/WKContextConfigurationPlayStation.cpp
    UIProcess/API/C/playstation/WKPagePrivatePlayStation.cpp
    UIProcess/API/C/playstation/WKRunLoop.cpp
    UIProcess/API/C/playstation/WKView.cpp

    UIProcess/API/C/crt/WKViewCRT.cpp

    UIProcess/CoordinatedGraphics/DrawingAreaProxyCoordinatedGraphics.cpp

    UIProcess/Launcher/crt/ProcessLauncherCRT.cpp

    UIProcess/linux/MemoryPressureMonitor.cpp

    UIProcess/WebsiteData/playstation/WebsiteDataStorePlayStation.cpp

    UIProcess/libwpe/WebPasteboardProxyLibWPE.cpp

    UIProcess/playstation/DisplayLinkPlayStation.cpp
    UIProcess/playstation/PageClientImpl.cpp
    UIProcess/playstation/PlayStationWebView.cpp
    UIProcess/playstation/WebPageProxyPlayStation.cpp
    UIProcess/playstation/WebPreferencesPlayStation.cpp
    UIProcess/playstation/WebProcessPoolPlayStation.cpp

    WebProcess/InjectedBundle/crt/InjectedBundleCRT.cpp

    WebProcess/WebPage/crt/DrawingAreaCRT.cpp

    WebProcess/WebPage/playstation/WebPagePlayStation.cpp

    WebProcess/playstation/WebProcessMainPlayStation.cpp
    WebProcess/playstation/WebProcessPlayStation.cpp
)

list(APPEND WebKit_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBKIT_DIR}/Platform/IPC/unix"
    "${WEBKIT_DIR}/Platform/classifier"
    "${WEBKIT_DIR}/Platform/generic"
    "${WEBKIT_DIR}/Shared/glib"
    "${WEBKIT_DIR}/Shared/libwpe"
    "${WEBKIT_DIR}/UIProcess/API/C/crt"
    "${WEBKIT_DIR}/UIProcess/API/C/playstation"
    "${WEBKIT_DIR}/UIProcess/API/libwpe"
    "${WEBKIT_DIR}/UIProcess/API/playstation"
    "${WEBKIT_DIR}/UIProcess/CoordinatedGraphics"
    "${WEBKIT_DIR}/UIProcess/linux"
    "${WEBKIT_DIR}/UIProcess/playstation"
    "${WEBKIT_DIR}/WebProcess/WebPage/CoordinatedGraphics"
    "${WEBKIT_DIR}/WebProcess/WebPage/libwpe"
)

if (USE_SKIA)
    include(Platform/Skia.cmake)

    list(APPEND WebKit_SOURCES
        UIProcess/skia/BackingStoreSkia.cpp
    )
endif ()

# No compositing stack in B4: the Web process draws through DrawingAreaCRT (WebPage/crt). Upstream's
# GLib/GL stack (LayerTreeHost, ThreadedCompositor, AcceleratedSurface) needs GLib, GBM or EGL
# contexts and is replaced, not adapted, until Tranche 9 brings the GPU.

list(APPEND WebKit_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBKIT_DIR}/WebProcess/WebPage/crt"
)

list(APPEND WebKit_LIBRARIES
    WPE::libwpe
)

list(APPEND WebKit_PUBLIC_FRAMEWORK_HEADERS
    Shared/API/c/playstation/WKBasePlayStation.h
    Shared/API/c/playstation/WKEventPlayStation.h

    UIProcess/API/C/crt/WKViewCRT.h

    UIProcess/API/C/playstation/WKContextConfigurationPlayStation.h
    UIProcess/API/C/playstation/WKPagePrivatePlayStation.h
    UIProcess/API/C/playstation/WKRunLoop.h
    UIProcess/API/C/playstation/WKView.h
    UIProcess/API/C/playstation/WKViewClient.h
)

# The B4 gate program (libcrtweb/tests/webkit): a UI process built on the C API, built only when
# the driver passes -DCRT_WEBKIT_TESTS=<dir>. The process executables sit next to it, which is
# where the CRT process launcher looks.
if (CRT_WEBKIT_TESTS)
    add_executable(crtweb_process_test ${CRT_WEBKIT_TESTS}/crtweb_process_test.cpp)
    target_include_directories(crtweb_process_test PRIVATE
        $<TARGET_PROPERTY:WebKit,INCLUDE_DIRECTORIES>
        ${WebKit_FRAMEWORK_HEADERS_DIR}
    )
    target_link_libraries(crtweb_process_test PRIVATE WebKit)
    add_dependencies(crtweb_process_test WebProcess NetworkProcess)

    add_executable(crtweb_surface_host ${CRT_WEBKIT_TESTS}/crtweb_surface_host.cpp)
    target_include_directories(crtweb_surface_host PRIVATE
        $<TARGET_PROPERTY:WebKit,INCLUDE_DIRECTORIES>
        ${WebKit_FRAMEWORK_HEADERS_DIR}
    )
    target_link_libraries(crtweb_surface_host PRIVATE WebKit)
    add_dependencies(crtweb_surface_host WebProcess NetworkProcess)

    add_executable(crtweb_lifecycle_test ${CRT_WEBKIT_TESTS}/crtweb_lifecycle_test.cpp)
    target_include_directories(crtweb_lifecycle_test PRIVATE
        $<TARGET_PROPERTY:WebKit,INCLUDE_DIRECTORIES>
        ${WebKit_FRAMEWORK_HEADERS_DIR}
    )
    target_link_libraries(crtweb_lifecycle_test PRIVATE WebKit)
    add_dependencies(crtweb_lifecycle_test WebProcess NetworkProcess)

    add_executable(crtweb_render_test ${CRT_WEBKIT_TESTS}/crtweb_render_test.cpp)
    target_include_directories(crtweb_render_test PRIVATE
        $<TARGET_PROPERTY:WebKit,INCLUDE_DIRECTORIES>
        ${WebKit_FRAMEWORK_HEADERS_DIR}
    )
    target_link_libraries(crtweb_render_test PRIVATE WebKit)
    add_dependencies(crtweb_render_test WebProcess NetworkProcess)
endif ()

# Web Tranche 4: libcrtweb, the CRT-owned runtime/view API (libcrtweb/include/crtweb/crtweb.h), built
# over this port when the driver passes -DCRT_LIBCRTWEB_DIR=<libcrtweb>. Only crtweb_* is exported; the
# test includes the public header alone (no WebKit header is on its include path).
if (CRT_LIBCRTWEB_DIR)
    add_library(crtweb SHARED ${CRT_LIBCRTWEB_DIR}/src/crtweb.cpp)
    target_include_directories(crtweb PRIVATE
        ${CRT_LIBCRTWEB_DIR}/include
        $<TARGET_PROPERTY:WebKit,INCLUDE_DIRECTORIES>
        ${WebKit_FRAMEWORK_HEADERS_DIR}
    )
    target_compile_definitions(crtweb PRIVATE CRTWEB_BUILD_LIBRARY=1)
    set_target_properties(crtweb PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
    target_link_libraries(crtweb PRIVATE WebKit)

    # The out-of-process half of the crtui WebView: one view over the web surface protocol, built from
    # the public API alone (and the 3A producer helper).
    add_executable(crtweb_host ${CRT_LIBCRTWEB_DIR}/tools/crtweb_host.cpp)
    target_include_directories(crtweb_host PRIVATE ${CRT_LIBCRTWEB_DIR}/include)
    target_link_libraries(crtweb_host PRIVATE crtweb)
    add_dependencies(crtweb_host WebProcess NetworkProcess)

    if (CRT_WEBKIT_TESTS)
        add_executable(crtweb_api_test ${CRT_WEBKIT_TESTS}/crtweb_api_test.cpp)
        target_include_directories(crtweb_api_test PRIVATE ${CRT_LIBCRTWEB_DIR}/include)
        target_link_libraries(crtweb_api_test PRIVATE crtweb)
        add_dependencies(crtweb_api_test WebProcess NetworkProcess)
    endif ()
endif ()
