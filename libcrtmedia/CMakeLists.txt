# FFmpeg-backed demux/decode bridge (crtmedia/demux.h, src/demux.c) --
# opt-in, off by default, matching libcrtgfx's own CRTGFX_ENABLE_SKIA
# pattern exactly (same reasoning: a plain default configure should not
# suddenly need a slow third-party fetch+build just to compile this
# library's own base sources). porting/recipes/ffmpeg.json's own
# port-build-ffmpeg target (auto-registered by the top-level CMakeLists.
# txt for every recipe under CRT_PORT_RECIPE_ROOT, no per-recipe CMake
# code needed) installs into the same shared port-tests/install prefix
# every other porting recipe here uses (freetype, wayland's expat/
# libffi, ...) -- see docs/porting_status.md.
set(CRTMEDIA_FFMPEG_PORT_PREFIX "${CMAKE_BINARY_DIR}/port-tests/install" CACHE PATH
  "Shared porting/recipes install prefix (ffmpeg) for libcrtmedia's demux/decode bridge")
set(CRTMEDIA_FFMPEG_LIBRARIES "")
if(EXISTS "${CRTMEDIA_FFMPEG_PORT_PREFIX}/lib/libavformat.a" AND
   EXISTS "${CRTMEDIA_FFMPEG_PORT_PREFIX}/lib/libavcodec.a" AND
   EXISTS "${CRTMEDIA_FFMPEG_PORT_PREFIX}/lib/libswresample.a" AND
   EXISTS "${CRTMEDIA_FFMPEG_PORT_PREFIX}/lib/libavutil.a")
  # Link order matters for a plain (non-cyclic-group) static link: a
  # library only satisfies symbols requested by something *already*
  # queued before it, so this lists each library before whichever
  # earlier one actually calls into it (avformat -> avcodec -> swresample
  # -> avutil, matching FFmpeg's own real internal dependency direction).
  set(CRTMEDIA_FFMPEG_LIBRARIES
    "${CRTMEDIA_FFMPEG_PORT_PREFIX}/lib/libavformat.a"
    "${CRTMEDIA_FFMPEG_PORT_PREFIX}/lib/libavcodec.a"
    "${CRTMEDIA_FFMPEG_PORT_PREFIX}/lib/libswresample.a"
    "${CRTMEDIA_FFMPEG_PORT_PREFIX}/lib/libavutil.a"
  )
endif()
option(CRTMEDIA_ENABLE_FFMPEG "Build libcrtmedia's FFmpeg-backed demux/decode bridge when available" OFF)
if(CRTMEDIA_ENABLE_FFMPEG AND NOT EXISTS "${CRTMEDIA_FFMPEG_PORT_PREFIX}/include/libavformat/avformat.h")
  message(FATAL_ERROR "CRTMEDIA_ENABLE_FFMPEG=ON but ${CRTMEDIA_FFMPEG_PORT_PREFIX}/include/libavformat/avformat.h was not found")
endif()
if(CRTMEDIA_ENABLE_FFMPEG AND NOT CRTMEDIA_FFMPEG_LIBRARIES)
  message(FATAL_ERROR "CRTMEDIA_ENABLE_FFMPEG=ON requires CRTMEDIA_FFMPEG_LIBRARIES. Run the port-build-ffmpeg target, then reconfigure.")
endif()

# crtmedia_backend_objects: real per-host audio output backends for
# crtmedia/audio_sink.h (TODO.md's upper-runtime roadmap "Build the
# software player" step -- "host audio sinks: WASAPI, CoreAudio, and
# PipeWire/ALSA"). Mirrors libcrtgfx's own crtgfx_backend_objects OBJECT
# library (libcrtgfx/CMakeLists.txt, ~line 807), including that target's
# own Linux-only crt_build_flags exception below -- src/arch/windows/
# audio_sink_wasapi.c hand-rolls every COM type/vtable it needs (see that
# file's own top comment) rather than #include-ing any host SDK header,
# so it is deliberately compiled with only this library's own include/src
# on its path, not this project's own libc, to keep that isolation real
# rather than accidental; src/arch/linux/audio_sink_linux.c has no such
# concern (this project's own libc *is* the right Linux syscall surface,
# see that file's own top comment) and needs crt_build_flags for its own
# real <time.h>/<sys/ioctl.h>/<sys/socket.h>/... includes to resolve at
# all. src/arch/macos/audio_sink_coreaudio.c (real CoreAudio AudioQueue)
# also uses this project's own libc headers normally, same as Linux's own
# audio_sink_linux.c -- there is no Windows-style header-collision concern
# on macOS either (CoreAudio's own hand-declared types do not collide with
# this project's own headers), and that file genuinely needs a real
# pthread_mutex_t/pthread_cond_t for its own buffer-pool bookkeeping (see
# that file's own top comment for why locking/signaling this project's own
# mutex/cond from a real foreign CoreAudio-spawned thread is safe).
set(CRTMEDIA_BACKEND_SOURCES "")
if(CRT_TARGET_OS STREQUAL "windows")
  list(APPEND CRTMEDIA_BACKEND_SOURCES src/arch/windows/audio_sink_wasapi.c)
elseif(CRT_TARGET_OS STREQUAL "linux")
  # src/arch/linux/audio_sink_linux.c: raw ALSA kernel PCM ioctl, falling
  # back to a hand-rolled real PulseAudio native-protocol client -- see
  # that file's own top comment for the full reasoning (both transports
  # confirmed for real; no libasound/libpulse link, matching this
  # library's own WASAPI backend precedent of never linking a host client
  # library). Uses this project's own libc headers normally (unlike
  # audio_sink_wasapi.c's own deliberate no-libc isolation) -- there is no
  # Windows-style "avoid the host SDK" concern on Linux, since this
  # project's own libc *is* the appropriate Linux syscall surface; only
  # the ALSA kernel UAPI struct/ioctl layout and the PulseAudio wire
  # protocol itself (neither part of this project's own bundled headers)
  # are hand-transcribed within that file.
  list(APPEND CRTMEDIA_BACKEND_SOURCES src/arch/linux/audio_sink_linux.c)
elseif(CRT_TARGET_OS STREQUAL "macos")
  # src/arch/macos/audio_sink_coreaudio.c: real CoreAudio AudioQueue
  # playback -- see that file's own top comment for the full reasoning
  # (every type/constant/function hand-declared from this exact machine's
  # own real SDK headers, cross-checked against a real throwaway probe on
  # real hardware before landing here) and CRTMEDIA_MACOS_FRAMEWORKS below
  # for the real -framework link this file's own AudioQueue calls need.
  list(APPEND CRTMEDIA_BACKEND_SOURCES src/arch/macos/audio_sink_coreaudio.c)
endif()
set(CRTMEDIA_BACKEND_OBJECTS "")
if(CRTMEDIA_BACKEND_SOURCES)
  add_library(crtmedia_backend_objects OBJECT ${CRTMEDIA_BACKEND_SOURCES})
  if(CRT_TARGET_OS STREQUAL "linux" OR CRT_TARGET_OS STREQUAL "macos")
    target_link_libraries(crtmedia_backend_objects PRIVATE crt_build_flags)
  endif()
  target_include_directories(crtmedia_backend_objects PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_include_directories(crtmedia_backend_objects PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  set_target_properties(crtmedia_backend_objects PROPERTIES
    POSITION_INDEPENDENT_CODE ON
  )
  set(CRTMEDIA_BACKEND_OBJECTS $<TARGET_OBJECTS:crtmedia_backend_objects>)
endif()

if(CRT_TARGET_OS STREQUAL "windows")
  # ole32.lib: the real flat-export import library for CoInitializeEx()/
  # CoCreateInstance()/CoUninitialize() (src/arch/windows/audio_sink_wasapi.c's
  # own COM bootstrapping) -- same find_file()-with-CACHE-fallback pattern
  # as libcrtgfx/CMakeLists.txt's own CRTGFX_WINDOWS_D3D11_LIB (~line 910).
  find_file(CRTMEDIA_WINDOWS_OLE32_LIB
    NAMES ole32.lib
    PATHS ${CRT_WINDOWS_SDK_LIB_CANDIDATES}
    NO_DEFAULT_PATH
  )
  if(NOT CRTMEDIA_WINDOWS_OLE32_LIB)
    set(CRTMEDIA_WINDOWS_OLE32_LIB ole32.lib CACHE FILEPATH "Windows ole32 import library")
  endif()
endif()

# src/arch/macos/audio_sink_coreaudio.c drives real CoreAudio from plain C
# without #include-ing any host SDK header (that file's own top comment) --
# the AudioToolbox framework still needs to be on the link line, the same
# way libcrtgfx's own CRTGFX_MACOS_FRAMEWORKS (that file's own ~line 943)
# puts Foundation/AppKit/QuartzCore/CoreGraphics/objc on window_cocoa.c's
# link line for the identical reason: a static archive member with
# undefined AudioToolbox symbols only resolves once something links this
# framework in, and crtmedia_shared (a real .dylib) needs it resolved at
# its own link time regardless.
if(CRT_TARGET_OS STREQUAL "macos")
  set(CRTMEDIA_MACOS_FRAMEWORKS "-framework AudioToolbox")
endif()

add_library(crtmedia STATIC
  src/runtime.c
  src/frame.c
  src/frame_convert.c
  src/audio.c
  src/format.c
  src/player.c
  ${CRTMEDIA_BACKEND_OBJECTS}
)
target_link_libraries(crtmedia PRIVATE crt_build_flags)
if(CRT_TARGET_OS STREQUAL "windows")
  # PUBLIC, not PRIVATE: matches crtgfx's own identical reasoning (that
  # target's own CRTGFX_WINDOWS_D3D11_LIB/_DXGI_LIB linking, ~line 1038) --
  # crtmedia is a plain static archive, so anything that links it (every
  # crtmedia_*_test target below, none of which mention ole32.lib
  # themselves) needs this forwarded transitively or the final executable
  # link fails with unresolved CoInitializeEx/CoCreateInstance/
  # CoUninitialize symbols.
  target_link_libraries(crtmedia PUBLIC "${CRTMEDIA_WINDOWS_OLE32_LIB}")
endif()
if(CRT_TARGET_OS STREQUAL "macos")
  # PUBLIC, matching the Windows ole32.lib case just above for the same
  # "a plain static archive needs this forwarded to whatever finally
  # links it" reason.
  target_link_libraries(crtmedia PUBLIC ${CRTMEDIA_MACOS_FRAMEWORKS})
endif()
if(CRT_TARGET_OS STREQUAL "linux" AND CRTMEDIA_ENABLE_FFMPEG)
  # --start-group/--end-group: the exact same real GNU-ld circular-
  # archive-resolution gap libcrtgfx's own crtgfx target already
  # documents and fixes this way (see that target's own fuller comment,
  # same file shape) -- confirmed for real (2026-08-31) via crtmedia_
  # demux_test's own generated link command: FFmpeg's own libavutil.a/
  # libswresample.a always land *after* c/m on the final link line
  # regardless of target_link_libraries() call order here, so ld.bfd's
  # default single left-to-right archive pass already has libm.a/
  # libc.a's own sin/cos/memchr/strspn/... behind it by the time it
  # reaches FFmpeg's own object files that need them -- every one comes
  # back "undefined reference" even though libm.a/libc.a both genuinely
  # define them.
  #
  # A first attempt wrapped only "dl cxx" + CRTMEDIA_FFMPEG_LIBRARIES in
  # the group, with "c m" added in a separate, earlier
  # target_link_libraries() call (still nominally "before" the group
  # markers in source order) -- confirmed for real this does NOT work:
  # CMake does not preserve that inter-call ordering faithfully once
  # transitive INTERFACE_LINK_LIBRARIES propagation is involved (ninja
  # -t commands showed libc.a/libm.a hoisted entirely *outside* (before)
  # the --start-group/--end-group pair, with only a second, duplicate
  # libc.a landing inside it). crtgfx's own working precedent (this
  # file's sibling, ~line 992-1006) puts its *entire* PRIVATE static-lib
  # set -- CRTGFX_CRT_STATIC_LIBS (c/m/dl/cxx equivalent) included --
  # inside one single group, not split across calls; matched exactly
  # here: c/m/dl/cxx and the FFmpeg archives are now all inside the same
  # start-group/end-group pair, in one contiguous sequence of calls.
  target_link_libraries(crtmedia PRIVATE -Wl,--start-group)
  target_link_libraries(crtmedia PRIVATE c m dl cxx)
  target_link_libraries(crtmedia PRIVATE ${CRTMEDIA_FFMPEG_LIBRARIES})
  target_link_libraries(crtmedia PRIVATE -Wl,--end-group)
else()
  target_link_libraries(crtmedia PRIVATE c m dl cxx)
  if(CRTMEDIA_ENABLE_FFMPEG)
    # macOS/Windows: no --start-group equivalent needed/used here yet
    # (matching crtgfx's own Linux-only branch for the identical
    # situation) -- plain ordering, not yet verified on either host for
    # this specific FFmpeg-vs-libm/libc case; revisit if/when a real
    # build surfaces the same undefined-reference gap there.
    target_link_libraries(crtmedia PRIVATE ${CRTMEDIA_FFMPEG_LIBRARIES})
  endif()
endif()
target_include_directories(crtmedia PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
set_target_properties(crtmedia PROPERTIES
  OUTPUT_NAME crtmedia
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

add_library(crtmedia_shared SHARED
  src/runtime.c
  src/frame.c
  src/frame_convert.c
  src/audio.c
  src/format.c
  src/player.c
  ${CRTMEDIA_BACKEND_OBJECTS}
)
if(CRT_TARGET_OS STREQUAL "windows")
  target_sources(crtmedia_shared PRIVATE $<TARGET_OBJECTS:crt_windows_dllcrt>)
endif()
target_link_libraries(crtmedia_shared PRIVATE crt_build_flags c_shared m_shared dl_shared cxx_shared)
if(CRT_TARGET_OS STREQUAL "windows")
  # Unlike crtmedia's own static archive (where a PUBLIC link on an
  # unresolved symbol simply defers resolution to whatever links it later),
  # crtmedia_shared is a real DLL -- Windows requires every symbol resolved
  # at this target's own link time, so ole32.lib must be linked here
  # directly too (matches crtmedia's own reasoning just above, and the
  # FFmpeg CRTMEDIA_FFMPEG_LIBRARIES/crtmedia_shared block's own identical
  # static-vs-shared distinction further down this file).
  target_link_libraries(crtmedia_shared PRIVATE "${CRTMEDIA_WINDOWS_OLE32_LIB}")
endif()
if(CRT_TARGET_OS STREQUAL "macos")
  # crtmedia_shared is a real .dylib -- needs AudioToolbox resolved at its
  # own link time too, matching crtmedia's own reasoning just above.
  target_link_libraries(crtmedia_shared PRIVATE ${CRTMEDIA_MACOS_FRAMEWORKS})
endif()
target_include_directories(crtmedia_shared PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
crt_configure_shared_runtime(crtmedia_shared)
set_target_properties(crtmedia_shared PROPERTIES
  OUTPUT_NAME crtmedia
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)
if(CRT_TARGET_OS STREQUAL "windows")
  set_target_properties(crtmedia_shared PROPERTIES
    ARCHIVE_OUTPUT_NAME crtmedia_dll
  )
endif()

if(CRTMEDIA_ENABLE_FFMPEG)
  # No add_dependencies(... port-build-ffmpeg) here -- confirmed for real
  # (2026-08-31) this creates a genuine CMake target-graph cycle: the
  # top-level "sysroot" target depends on crtmedia/crtmedia_shared (to
  # stage them in), while port-build-ffmpeg itself depends on "sysroot"
  # (an ordinary third-party port, built *against* the already-staged
  # sysroot, exactly like every other porting/recipes/*.json target) --
  # so crtmedia -> port-build-ffmpeg -> sysroot -> crtmedia is circular.
  # The CRTMEDIA_FFMPEG_LIBRARIES/EXISTS guard above already enforces the
  # real ordering at *configure* time (CRTMEDIA_ENABLE_FFMPEG=ON refuses
  # to configure at all until port-build-ffmpeg has already been run
  # once) -- matching libcrtgfx's own CRTGFX_ENABLE_SKIA precedent
  # exactly, which uses the identical configure-time-guard-only pattern
  # for the same reason.
  # src/extractor.c/src/codec.c: docs/libcrtmedia_api_policy.md's decided
  # core (crtmedia_format/crtmedia_extractor/crtmedia_codec, TODO.md's
  # "Separate extractor and codec" item) -- src/demux.c (crtmedia_demuxer_*)
  # stays its own, separate, already-verified implementation for now, not
  # yet rebuilt over this new core (see that policy doc's own Decision).
  target_sources(crtmedia PRIVATE src/demux.c src/extractor.c src/codec.c)
  target_include_directories(crtmedia PRIVATE "${CRTMEDIA_FFMPEG_PORT_PREFIX}/include")
  # crtmedia's own PRIVATE link libraries call (including, on Linux, the
  # --start-group/--end-group-wrapped CRTMEDIA_FFMPEG_LIBRARIES) already
  # happened right after add_library(crtmedia ...) above -- see that
  # call's own comment for why it has to live there and not here.

  target_sources(crtmedia_shared PRIVATE src/demux.c src/extractor.c src/codec.c)
  target_include_directories(crtmedia_shared PRIVATE "${CRTMEDIA_FFMPEG_PORT_PREFIX}/include")
  # Unlike crtmedia's own static archive (where an unresolved FFmpeg
  # symbol is simply left for whatever final executable links crtmedia
  # to resolve), crtmedia_shared is a real DLL/shared object -- Windows
  # in particular requires every symbol resolved at link time (no lazy
  # binding the way ELF .so allows), so this shared target must link the
  # FFmpeg archives directly or the DLL build itself fails.
  target_link_libraries(crtmedia_shared PRIVATE ${CRTMEDIA_FFMPEG_LIBRARIES})
endif()

install(TARGETS crtmedia crtmedia_shared
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include
  PATTERN ".gitkeep" EXCLUDE
)

# crtmedia_frame_test: deterministic, host-resource-free coverage for the
# CPU frame handoff contract (crtmedia/frame.h, TODO.md's "Define the
# libcrtmedia CPU frame handoff contract" item) -- plane geometry,
# ownership/release, and packed-RGB/YUV420P->RGBA conversion. Mirrors
# libcrtgfx's own crtgfx_synthetic_event target shape (tests/synthetic_
# event_test.c, libcrtgfx/CMakeLists.txt): same startup objects, same
# per-OS link options, same crt_link_startup_end_objects()/add_
# dependencies() calls, since both are plain C executables needing no
# real host window/display connection. The real-Skia counterpart (tests/
# frame_skia_smoke.cc) is built and registered from libcrtgfx/CMakeLists.
# txt instead, not here -- see that file's own comment on why (crt_wire_
# skia_executable() is a function() defined in that file's own directory
# scope, and CMake function definitions do not propagate to a sibling
# directory's CMakeLists.txt).
add_executable(crtmedia_frame_test
  ${CRT_STARTUP_OBJECTS}
  tests/frame_test.c
)
target_link_libraries(crtmedia_frame_test PRIVATE crt_build_flags c crtmedia)
target_include_directories(crtmedia_frame_test PRIVATE
  "${CMAKE_SOURCE_DIR}/libc/include"
)
target_link_libraries(crtmedia_frame_test PRIVATE crt_compiler_rt_builtins)
target_link_options(crtmedia_frame_test PRIVATE
  -nostdlib
  -nostartfiles
  -nodefaultlibs
)
if(CRT_TARGET_OS STREQUAL "macos")
  target_link_options(crtmedia_frame_test PRIVATE
    -Wl,-e,_start
    -Wl,-dead_strip
  )
  target_link_libraries(crtmedia_frame_test PRIVATE System)
elseif(CRT_TARGET_OS STREQUAL "linux")
  target_link_options(crtmedia_frame_test PRIVATE
    -Wl,-e,_start
    -Wl,--gc-sections
  )
elseif(CRT_TARGET_OS STREQUAL "windows")
  target_link_options(crtmedia_frame_test PRIVATE
    -Wl,/entry:mainCRTStartup
    -Wl,/subsystem:console
    -Wl,/OPT:REF
  )
  target_link_libraries(crtmedia_frame_test PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
endif()
crt_link_startup_end_objects(crtmedia_frame_test)
add_dependencies(crtmedia_frame_test c crt1 crtmedia)

add_test(NAME crtmedia_frame_test_runs COMMAND crtmedia_frame_test)
set_tests_properties(crtmedia_frame_test_runs PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PASS_REGULAR_EXPRESSION "crtmedia_frame_test: ok"
)
set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtmedia_frame_test_runs)

# crtmedia_format_test: deterministic, FFmpeg-free coverage for crtmedia/
# format.h (docs/libcrtmedia_api_policy.md's decided core, TODO.md's
# "Separate extractor and codec" item) -- unconditional, not gated behind
# CRTMEDIA_ENABLE_FFMPEG, since crtmedia_format itself has no FFmpeg
# dependency at all (see src/format.c's own top comment). Same plain-C-
# executable shape as crtmedia_frame_test just above.
add_executable(crtmedia_format_test
  ${CRT_STARTUP_OBJECTS}
  tests/format_test.c
)
target_link_libraries(crtmedia_format_test PRIVATE crt_build_flags c crtmedia)
target_include_directories(crtmedia_format_test PRIVATE
  "${CMAKE_SOURCE_DIR}/libc/include"
)
target_link_libraries(crtmedia_format_test PRIVATE crt_compiler_rt_builtins)
target_link_options(crtmedia_format_test PRIVATE
  -nostdlib
  -nostartfiles
  -nodefaultlibs
)
if(CRT_TARGET_OS STREQUAL "macos")
  target_link_options(crtmedia_format_test PRIVATE
    -Wl,-e,_start
    -Wl,-dead_strip
  )
  target_link_libraries(crtmedia_format_test PRIVATE System)
elseif(CRT_TARGET_OS STREQUAL "linux")
  target_link_options(crtmedia_format_test PRIVATE
    -Wl,-e,_start
    -Wl,--gc-sections
  )
elseif(CRT_TARGET_OS STREQUAL "windows")
  target_link_options(crtmedia_format_test PRIVATE
    -Wl,/entry:mainCRTStartup
    -Wl,/subsystem:console
    -Wl,/OPT:REF
  )
  target_link_libraries(crtmedia_format_test PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
endif()
crt_link_startup_end_objects(crtmedia_format_test)
add_dependencies(crtmedia_format_test c crt1 crtmedia)

add_test(NAME crtmedia_format_test_runs COMMAND crtmedia_format_test)
set_tests_properties(crtmedia_format_test_runs PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PASS_REGULAR_EXPRESSION "crtmedia_format_test: ok"
)
set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtmedia_format_test_runs)

# crtmedia_player_test: deterministic (real host clock, generous
# tolerance), FFmpeg-free coverage for crtmedia/player.h (TODO.md's
# upper-runtime roadmap "Build the software player" step) -- unconditional,
# not gated behind CRTMEDIA_ENABLE_FFMPEG, matching crtmedia_format_test's
# own reasoning (no FFmpeg dependency at all, see src/player.c's own top
# comment). Same plain-C-executable shape as crtmedia_frame_test/crtmedia_
# format_test above.
add_executable(crtmedia_player_test
  ${CRT_STARTUP_OBJECTS}
  tests/player_test.c
)
target_link_libraries(crtmedia_player_test PRIVATE crt_build_flags c crtmedia)
target_include_directories(crtmedia_player_test PRIVATE
  "${CMAKE_SOURCE_DIR}/libc/include"
)
target_link_libraries(crtmedia_player_test PRIVATE crt_compiler_rt_builtins)
target_link_options(crtmedia_player_test PRIVATE
  -nostdlib
  -nostartfiles
  -nodefaultlibs
)
if(CRT_TARGET_OS STREQUAL "macos")
  target_link_options(crtmedia_player_test PRIVATE
    -Wl,-e,_start
    -Wl,-dead_strip
  )
  target_link_libraries(crtmedia_player_test PRIVATE System)
elseif(CRT_TARGET_OS STREQUAL "linux")
  target_link_options(crtmedia_player_test PRIVATE
    -Wl,-e,_start
    -Wl,--gc-sections
  )
elseif(CRT_TARGET_OS STREQUAL "windows")
  target_link_options(crtmedia_player_test PRIVATE
    -Wl,/entry:mainCRTStartup
    -Wl,/subsystem:console
    -Wl,/OPT:REF
  )
  target_link_libraries(crtmedia_player_test PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
endif()
crt_link_startup_end_objects(crtmedia_player_test)
add_dependencies(crtmedia_player_test c crt1 crtmedia)

add_test(NAME crtmedia_player_test_runs COMMAND crtmedia_player_test)
set_tests_properties(crtmedia_player_test_runs PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  PASS_REGULAR_EXPRESSION "crtmedia_player_test: ok"
)
set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtmedia_player_test_runs)

# crtmedia_audio_sink_test: real coverage for crtmedia/audio_sink.h's real
# per-host backends (src/arch/windows/audio_sink_wasapi.c, src/arch/linux/
# audio_sink_linux.c, src/arch/macos/audio_sink_coreaudio.c, TODO.md's
# upper-runtime roadmap "Build the software player" step -- "host audio
# sinks") -- argument validation plus a real open/write/get_position/close
# cycle against the real default host audio device when one is available
# (see tests/audio_sink_test.c's own top comment for the graceful-
# degradation contract when it is not). Built whenever a real backend
# exists (CRTMEDIA_BACKEND_SOURCES above). Same plain-C-executable shape as
# crtmedia_frame_test above, no FFmpeg dependency (unconditional on
# CRTMEDIA_ENABLE_FFMPEG).
if(CRTMEDIA_BACKEND_SOURCES)
  add_executable(crtmedia_audio_sink_test
    ${CRT_STARTUP_OBJECTS}
    tests/audio_sink_test.c
  )
  target_link_libraries(crtmedia_audio_sink_test PRIVATE crt_build_flags c crtmedia)
  target_include_directories(crtmedia_audio_sink_test PRIVATE
    "${CMAKE_SOURCE_DIR}/libc/include"
  )
  target_link_libraries(crtmedia_audio_sink_test PRIVATE crt_compiler_rt_builtins)
  target_link_options(crtmedia_audio_sink_test PRIVATE
    -nostdlib
    -nostartfiles
    -nodefaultlibs
  )
  if(CRT_TARGET_OS STREQUAL "linux")
    target_link_options(crtmedia_audio_sink_test PRIVATE
      -Wl,-e,_start
      -Wl,--gc-sections
    )
  elseif(CRT_TARGET_OS STREQUAL "macos")
    target_link_options(crtmedia_audio_sink_test PRIVATE
      -Wl,-e,_start
      -Wl,-dead_strip
    )
    # Explicit c/m/dl/cxx before System, matching crtmedia_demux_test's own
    # identical fix just above in this same file (that target's own
    # comment has the full real pthread_once/-lSystem ABI-mismatch
    # writeup) -- this project's own pthread_mutex_t/pthread_cond_t
    # (audio_sink_coreaudio.c's own buffer-pool lock) need to resolve to
    # this project's own pthread.c, not get shadowed by real Darwin's.
    target_link_libraries(crtmedia_audio_sink_test PRIVATE c m dl cxx)
    target_link_libraries(crtmedia_audio_sink_test PRIVATE System)
  elseif(CRT_TARGET_OS STREQUAL "windows")
    target_link_options(crtmedia_audio_sink_test PRIVATE
      -Wl,/entry:mainCRTStartup
      -Wl,/subsystem:console
      -Wl,/OPT:REF
    )
    target_link_libraries(crtmedia_audio_sink_test PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
  endif()
  crt_link_startup_end_objects(crtmedia_audio_sink_test)
  add_dependencies(crtmedia_audio_sink_test c crt1 crtmedia)

  add_test(NAME crtmedia_audio_sink_test_runs COMMAND crtmedia_audio_sink_test)
  set_tests_properties(crtmedia_audio_sink_test_runs PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    PASS_REGULAR_EXPRESSION "crtmedia_audio_sink_test: ok"
  )
  set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtmedia_audio_sink_test_runs)
endif()

# crtmedia_demux_test: real FFmpeg demux/decode coverage (crtmedia/demux.h,
# src/demux.c) -- opens libcrtmedia/assets/test_tone.wav (a real, tiny,
# project-authored WAV fixture -- see assets/README.md) and checks real
# stream info and decoded PCM sample counts, not just that the calls
# succeed without crashing. Gated behind CRTMEDIA_ENABLE_FFMPEG exactly
# like the library's own demux.c sources above (same reasoning: needs a
# real port-build-ffmpeg first). Same plain-C-executable shape as
# crtmedia_frame_test just above.
if(CRTMEDIA_ENABLE_FFMPEG)
  add_executable(crtmedia_demux_test
    ${CRT_STARTUP_OBJECTS}
    tests/demux_decode_test.c
  )
  # Deliberately NOT linking c/m directly here (unlike crtmedia_frame_
  # test just above): crtmedia itself now links c/m/dl/cxx PRIVATE inside
  # its own --start-group/--end-group pair (see add_library(crtmedia ...)
  # above). Confirmed for real (2026-08-31) that listing "c m" directly
  # on *this* target's own link line -- even positioned after crtmedia --
  # makes CMake's link-line generator resolve c/m's position from this
  # call site (ahead of crtmedia's own forwarded, grouped libraries)
  # rather than from inside crtmedia's group, which reintroduces the
  # exact same single-pass archive-scan-order bug the group exists to
  # fix (FFmpeg's own sin/cos/memchr/... references come back "undefined
  # reference" again despite libm.a/libc.a genuinely defining them).
  # Omitting c/m here lets crtmedia's own PRIVATE link libraries (STATIC
  # library link libraries are still forwarded to consumers by CMake --
  # only *usage requirements* like include dirs are not -- so this does
  # not lose them) surface at the position crtmedia's own group markers
  # put them.
  target_link_libraries(crtmedia_demux_test PRIVATE crt_build_flags crtmedia)
  target_include_directories(crtmedia_demux_test PRIVATE
    "${CMAKE_SOURCE_DIR}/libc/include"
  )
  target_compile_definitions(crtmedia_demux_test PRIVATE
    CRTMEDIA_TEST_WAV_PATH="${CMAKE_SOURCE_DIR}/libcrtmedia/assets/test_tone.wav"
  )
  target_link_libraries(crtmedia_demux_test PRIVATE crt_compiler_rt_builtins)
  target_link_options(crtmedia_demux_test PRIVATE
    -nostdlib
    -nostartfiles
    -nodefaultlibs
  )
  if(CRT_TARGET_OS STREQUAL "macos")
    target_link_options(crtmedia_demux_test PRIVATE
      -Wl,-e,_start
      -Wl,-dead_strip
    )
    # Explicit c/m/dl/cxx here, unlike the Linux branch's own deliberate
    # omission just below (see that comment for why omitting them there
    # is required to keep crtmedia's own --start-group/--end-group intact)
    # -- macOS has no such group to disturb (crtmedia's own else() branch,
    # add_library(crtmedia ...) above, links c/m/dl/cxx plainly, no
    # grouping), and a real, different bug needs this here instead.
    # Confirmed for real (2026-09-01): without this, crtmedia_demux_test
    # crashed with EXC_BAD_ACCESS/Bus error inside real Darwin
    # libsystem_pthread.dylib's own pthread_once(), called from FFmpeg's
    # avcodec_find_decoder() -> av_codec_static_init's one-time
    # registration guard. Root cause: this executable's own final link
    # line put `-lSystem` (added by the target_link_libraries(...
    # System) call just below) *before* crtmedia's own propagated PRIVATE
    # c/m/dl/cxx (which land wherever CMake appends a linked static
    # target's own forwarded dependencies -- confirmed, via `ninja -v`,
    # to be *after* every directly-listed library including System, not
    # at crtmedia's own position in the call sequence). Apple ld's
    # dynamic-library symbol resolution does not care about archive scan
    # order the way a static archive does: once `-lSystem` is reached
    # with `pthread_once` still unresolved, its real, exported
    # implementation satisfies the reference immediately, and this
    # project's own `pthread_once` (libc/src/pthread.c, later in the
    # link line inside `libc.a`) is never even consulted -- FFmpeg's
    # `codec_mutex` was allocated using *this project's own*, much
    # smaller `pthread_once_t` layout (from this project's own
    # <pthread.h>, the only one crt-cc's own -nostdinc sysroot exposes),
    # but the function that actually ran against it was real Darwin's,
    # which expects its own, differently-sized opaque `pthread_once_t` --
    # a real ABI mismatch, not a logic bug, causing an out-of-bounds/
    # misaligned atomic access. Mirrors crtmedia_frame_test's own
    # existing, already-working `c` link just above in this same file --
    # extended here to `m`/`dl`/`cxx` too, not just `c`, since the same
    # shadow-by-libSystem risk exists for whichever of that set FFmpeg's
    # own code happens to call into. Placed *before* the System link
    # immediately below, matching frame_test's own "the CRT library
    # before whatever host fallback comes after" ordering intent.
    target_link_libraries(crtmedia_demux_test PRIVATE c m dl cxx)
    target_link_libraries(crtmedia_demux_test PRIVATE System)
  elseif(CRT_TARGET_OS STREQUAL "linux")
    target_link_options(crtmedia_demux_test PRIVATE
      -Wl,-e,_start
      -Wl,--gc-sections
    )
  elseif(CRT_TARGET_OS STREQUAL "windows")
    target_link_options(crtmedia_demux_test PRIVATE
      -Wl,/entry:mainCRTStartup
      -Wl,/subsystem:console
      -Wl,/OPT:REF
    )
    target_link_libraries(crtmedia_demux_test PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
  endif()
  crt_link_startup_end_objects(crtmedia_demux_test)
  add_dependencies(crtmedia_demux_test c crt1 crtmedia)

  add_test(NAME crtmedia_demux_test_runs COMMAND crtmedia_demux_test)
  set_tests_properties(crtmedia_demux_test_runs PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    PASS_REGULAR_EXPRESSION "crtmedia_demux_test: ok"
  )
  set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtmedia_demux_test_runs)
endif()

# crtmedia_demux_video_test / crtmedia_demux_mp3_test / crtmedia_demux_
# malformed_test: TODO.md's upper-runtime roadmap step 1 ("Complete
# software-decode evidence") -- real H.264+AAC/MP3 fixture coverage
# (assets/test_video.mp4, assets/test_tone.mp3, see assets/README.md),
# threaded H.264 decode (src/demux.c's own explicit thread_count=2 for
# video streams), PTS ordering, EOF drain/flush, and malformed-input
# handling. Same plain-C-executable shape and same per-OS link-order
# reasoning as crtmedia_demux_test just above (see that block's own
# comments for why crtmedia's own PRIVATE link libraries are handled the
# way they are on each host) -- duplicated per test target rather than
# factored into a shared CMake function, matching this file's own
# existing crtmedia_frame_test/crtmedia_demux_test precedent (no shared
# "build a startup-object test executable" helper exists anywhere in this
# codebase to reuse).
if(CRTMEDIA_ENABLE_FFMPEG)
  add_executable(crtmedia_demux_video_test
    ${CRT_STARTUP_OBJECTS}
    tests/demux_decode_video_test.c
  )
  target_link_libraries(crtmedia_demux_video_test PRIVATE crt_build_flags crtmedia)
  target_include_directories(crtmedia_demux_video_test PRIVATE
    "${CMAKE_SOURCE_DIR}/libc/include"
  )
  target_compile_definitions(crtmedia_demux_video_test PRIVATE
    CRTMEDIA_TEST_VIDEO_PATH="${CMAKE_SOURCE_DIR}/libcrtmedia/assets/test_video.mp4"
  )
  target_link_libraries(crtmedia_demux_video_test PRIVATE crt_compiler_rt_builtins)
  target_link_options(crtmedia_demux_video_test PRIVATE
    -nostdlib
    -nostartfiles
    -nodefaultlibs
  )
  if(CRT_TARGET_OS STREQUAL "macos")
    target_link_options(crtmedia_demux_video_test PRIVATE
      -Wl,-e,_start
      -Wl,-dead_strip
    )
    # See crtmedia_demux_test's own matching block above for the full
    # pthread_once/-lSystem-ordering reasoning this mirrors.
    target_link_libraries(crtmedia_demux_video_test PRIVATE c m dl cxx)
    target_link_libraries(crtmedia_demux_video_test PRIVATE System)
  elseif(CRT_TARGET_OS STREQUAL "linux")
    target_link_options(crtmedia_demux_video_test PRIVATE
      -Wl,-e,_start
      -Wl,--gc-sections
    )
  elseif(CRT_TARGET_OS STREQUAL "windows")
    target_link_options(crtmedia_demux_video_test PRIVATE
      -Wl,/entry:mainCRTStartup
      -Wl,/subsystem:console
      -Wl,/OPT:REF
    )
    target_link_libraries(crtmedia_demux_video_test PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
  endif()
  crt_link_startup_end_objects(crtmedia_demux_video_test)
  add_dependencies(crtmedia_demux_video_test c crt1 crtmedia)

  add_test(NAME crtmedia_demux_video_test_runs COMMAND crtmedia_demux_video_test)
  set_tests_properties(crtmedia_demux_video_test_runs PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    PASS_REGULAR_EXPRESSION "crtmedia_demux_video_test: ok"
  )
  set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtmedia_demux_video_test_runs)

  add_executable(crtmedia_demux_mp3_test
    ${CRT_STARTUP_OBJECTS}
    tests/demux_decode_mp3_test.c
  )
  target_link_libraries(crtmedia_demux_mp3_test PRIVATE crt_build_flags crtmedia)
  target_include_directories(crtmedia_demux_mp3_test PRIVATE
    "${CMAKE_SOURCE_DIR}/libc/include"
  )
  target_compile_definitions(crtmedia_demux_mp3_test PRIVATE
    CRTMEDIA_TEST_MP3_PATH="${CMAKE_SOURCE_DIR}/libcrtmedia/assets/test_tone.mp3"
  )
  target_link_libraries(crtmedia_demux_mp3_test PRIVATE crt_compiler_rt_builtins)
  target_link_options(crtmedia_demux_mp3_test PRIVATE
    -nostdlib
    -nostartfiles
    -nodefaultlibs
  )
  if(CRT_TARGET_OS STREQUAL "macos")
    target_link_options(crtmedia_demux_mp3_test PRIVATE
      -Wl,-e,_start
      -Wl,-dead_strip
    )
    target_link_libraries(crtmedia_demux_mp3_test PRIVATE c m dl cxx)
    target_link_libraries(crtmedia_demux_mp3_test PRIVATE System)
  elseif(CRT_TARGET_OS STREQUAL "linux")
    target_link_options(crtmedia_demux_mp3_test PRIVATE
      -Wl,-e,_start
      -Wl,--gc-sections
    )
  elseif(CRT_TARGET_OS STREQUAL "windows")
    target_link_options(crtmedia_demux_mp3_test PRIVATE
      -Wl,/entry:mainCRTStartup
      -Wl,/subsystem:console
      -Wl,/OPT:REF
    )
    target_link_libraries(crtmedia_demux_mp3_test PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
  endif()
  crt_link_startup_end_objects(crtmedia_demux_mp3_test)
  add_dependencies(crtmedia_demux_mp3_test c crt1 crtmedia)

  add_test(NAME crtmedia_demux_mp3_test_runs COMMAND crtmedia_demux_mp3_test)
  set_tests_properties(crtmedia_demux_mp3_test_runs PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    PASS_REGULAR_EXPRESSION "crtmedia_demux_mp3_test: ok"
  )
  set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtmedia_demux_mp3_test_runs)

  add_executable(crtmedia_demux_malformed_test
    ${CRT_STARTUP_OBJECTS}
    tests/demux_decode_malformed_test.c
  )
  target_link_libraries(crtmedia_demux_malformed_test PRIVATE crt_build_flags crtmedia)
  target_include_directories(crtmedia_demux_malformed_test PRIVATE
    "${CMAKE_SOURCE_DIR}/libc/include"
  )
  target_compile_definitions(crtmedia_demux_malformed_test PRIVATE
    CRTMEDIA_TEST_VIDEO_PATH="${CMAKE_SOURCE_DIR}/libcrtmedia/assets/test_video.mp4"
    CRTMEDIA_TEST_SCRATCH_DIR="${CMAKE_BINARY_DIR}"
  )
  target_link_libraries(crtmedia_demux_malformed_test PRIVATE crt_compiler_rt_builtins)
  target_link_options(crtmedia_demux_malformed_test PRIVATE
    -nostdlib
    -nostartfiles
    -nodefaultlibs
  )
  if(CRT_TARGET_OS STREQUAL "macos")
    target_link_options(crtmedia_demux_malformed_test PRIVATE
      -Wl,-e,_start
      -Wl,-dead_strip
    )
    target_link_libraries(crtmedia_demux_malformed_test PRIVATE c m dl cxx)
    target_link_libraries(crtmedia_demux_malformed_test PRIVATE System)
  elseif(CRT_TARGET_OS STREQUAL "linux")
    target_link_options(crtmedia_demux_malformed_test PRIVATE
      -Wl,-e,_start
      -Wl,--gc-sections
    )
  elseif(CRT_TARGET_OS STREQUAL "windows")
    target_link_options(crtmedia_demux_malformed_test PRIVATE
      -Wl,/entry:mainCRTStartup
      -Wl,/subsystem:console
      -Wl,/OPT:REF
    )
    target_link_libraries(crtmedia_demux_malformed_test PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
  endif()
  crt_link_startup_end_objects(crtmedia_demux_malformed_test)
  add_dependencies(crtmedia_demux_malformed_test c crt1 crtmedia)

  add_test(NAME crtmedia_demux_malformed_test_runs COMMAND crtmedia_demux_malformed_test)
  set_tests_properties(crtmedia_demux_malformed_test_runs PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    PASS_REGULAR_EXPRESSION "crtmedia_demux_malformed_test: ok"
  )
  set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtmedia_demux_malformed_test_runs)
endif()

# crtmedia_extractor_codec_test: real end-to-end coverage for docs/
# libcrtmedia_api_policy.md's decided core (crtmedia_extractor +
# crtmedia_codec, crtmedia/extractor.h + crtmedia/codec.h) driven together
# against the same real MP4 fixture crtmedia_demux_video_test already
# verifies through the older crtmedia_demuxer_* convenience API -- proving
# the new async buffer-queue core decodes the exact same real content
# correctly. Same gating/shape as crtmedia_demux_video_test above.
if(CRTMEDIA_ENABLE_FFMPEG)
  add_executable(crtmedia_extractor_codec_test
    ${CRT_STARTUP_OBJECTS}
    tests/extractor_codec_test.c
  )
  target_link_libraries(crtmedia_extractor_codec_test PRIVATE crt_build_flags crtmedia)
  target_include_directories(crtmedia_extractor_codec_test PRIVATE
    "${CMAKE_SOURCE_DIR}/libc/include"
  )
  target_compile_definitions(crtmedia_extractor_codec_test PRIVATE
    CRTMEDIA_TEST_VIDEO_PATH="${CMAKE_SOURCE_DIR}/libcrtmedia/assets/test_video.mp4"
  )
  target_link_libraries(crtmedia_extractor_codec_test PRIVATE crt_compiler_rt_builtins)
  target_link_options(crtmedia_extractor_codec_test PRIVATE
    -nostdlib
    -nostartfiles
    -nodefaultlibs
  )
  if(CRT_TARGET_OS STREQUAL "macos")
    target_link_options(crtmedia_extractor_codec_test PRIVATE
      -Wl,-e,_start
      -Wl,-dead_strip
    )
    # See crtmedia_demux_test's own matching block above for the full
    # pthread_once/-lSystem-ordering reasoning this mirrors.
    target_link_libraries(crtmedia_extractor_codec_test PRIVATE c m dl cxx)
    target_link_libraries(crtmedia_extractor_codec_test PRIVATE System)
  elseif(CRT_TARGET_OS STREQUAL "linux")
    target_link_options(crtmedia_extractor_codec_test PRIVATE
      -Wl,-e,_start
      -Wl,--gc-sections
    )
  elseif(CRT_TARGET_OS STREQUAL "windows")
    target_link_options(crtmedia_extractor_codec_test PRIVATE
      -Wl,/entry:mainCRTStartup
      -Wl,/subsystem:console
      -Wl,/OPT:REF
    )
    target_link_libraries(crtmedia_extractor_codec_test PRIVATE ${CRT_WINDOWS_SYSTEM_LIBS})
  endif()
  crt_link_startup_end_objects(crtmedia_extractor_codec_test)
  add_dependencies(crtmedia_extractor_codec_test c crt1 crtmedia)

  add_test(NAME crtmedia_extractor_codec_test_runs COMMAND crtmedia_extractor_codec_test)
  set_tests_properties(crtmedia_extractor_codec_test_runs PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    PASS_REGULAR_EXPRESSION "crtmedia_extractor_codec_test: ok"
  )
  set_property(GLOBAL APPEND PROPERTY CRT_REGISTERED_TESTS crtmedia_extractor_codec_test_runs)
endif()
