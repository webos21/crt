/* CRT VA-API stub for the web stage (Web Tranche 8B).
 *
 * libcrtmedia.so is linked against VA-API for its hardware decoder. The Web process plays media on the
 * CPU only (GPU decode and presentation are Tranche 9), and the Host ABI audit forbids a host library
 * in it. This stub library, installed as libva.so.2 and libva-drm.so.2 next to the web stage, satisfies
 * those dependencies: every entry point fails with VA_STATUS_ERROR_UNIMPLEMENTED (and the display
 * query returns NULL), so crtmedia reports "no hardware decoder" and decodes in software. It is
 * never a way to run hardware decode. Copyright (c) 2026 The CRT Project. BSD-2-Clause. */

#include <stddef.h>

#define VA_STATUS_ERROR_UNIMPLEMENTED 0x00000014

#define STUB_STATUS(name) int name() { return VA_STATUS_ERROR_UNIMPLEMENTED; }
#define STUB_INT(name) int name() { return 0; }
#define STUB_STRING(name) const char* name() { return "unavailable"; }

STUB_STATUS(vaBeginPicture)
STUB_STATUS(vaCreateBuffer)
STUB_STATUS(vaCreateConfig)
STUB_STATUS(vaCreateContext)
STUB_STATUS(vaCreateImage)
STUB_STATUS(vaCreateSurfaces)
STUB_STATUS(vaDeriveImage)
STUB_STATUS(vaDestroyBuffer)
STUB_STATUS(vaDestroyConfig)
STUB_STATUS(vaDestroyContext)
STUB_STATUS(vaDestroyImage)
STUB_STATUS(vaDestroySurfaces)
STUB_STATUS(vaEndPicture)
STUB_STATUS(vaExportSurfaceHandle)
STUB_STATUS(vaGetConfigAttributes)
STUB_STATUS(vaGetImage)
STUB_STATUS(vaInitialize)
STUB_STATUS(vaMapBuffer)
STUB_STATUS(vaMapBuffer2)
STUB_STATUS(vaPutImage)
STUB_STATUS(vaQueryConfigEntrypoints)
STUB_STATUS(vaQueryConfigProfiles)
STUB_STATUS(vaQueryImageFormats)
STUB_STATUS(vaQuerySurfaceAttributes)
STUB_STATUS(vaRenderPicture)
STUB_STATUS(vaSetDriverName)
STUB_STATUS(vaSyncBuffer)
STUB_STATUS(vaSyncSurface)
STUB_STATUS(vaTerminate)
STUB_STATUS(vaUnmapBuffer)

STUB_INT(vaMaxNumEntrypoints)
STUB_INT(vaMaxNumImageFormats)
STUB_INT(vaMaxNumProfiles)

STUB_STRING(vaEntrypointStr)
STUB_STRING(vaErrorStr)
STUB_STRING(vaProfileStr)
STUB_STRING(vaQueryVendorString)

void* vaGetDisplayDRM() { return NULL; }
void* vaSetErrorCallback() { return NULL; }
void* vaSetInfoCallback() { return NULL; }
