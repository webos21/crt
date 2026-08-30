#include "wayland_weston_internal.h"

#include <stddef.h>
#include <stdint.h>
/* Deliberately no <string.h> (no memset): this translation unit compiles
 * with only libcrtgfx/include and libcrtgfx/src on its own include path
 * (see libcrtgfx/CMakeLists.txt's own crtgfx_backend_objects target),
 * not this project's own libc headers -- matching how everything else in
 * this file already avoids the platform libc (WNDCLASSEXA/CREATESTRUCTA/
 * etc. are all hand-declared above, no <windows.h>). `= {0}` compound-
 * literal zero-init (already used by crtgfx_register_window_class()'s
 * own `cls = (WNDCLASSEXA){0}`, just below) is a real C99 language
 * feature, not a libc function -- used the same way for every
 * crtgfx_event below instead of memset(). */

#if defined(_M_IX86) || defined(__i386__)
#define CRTGFX_WINAPI __stdcall
#else
#define CRTGFX_WINAPI
#endif

typedef int BOOL;
typedef unsigned short ATOM;
typedef unsigned int UINT;
typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef long LONG;
typedef intptr_t LONG_PTR;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
typedef intptr_t LRESULT;
typedef void* HANDLE;
typedef HANDLE HBRUSH;
typedef HANDLE HCURSOR;
typedef HANDLE HDC;
typedef HANDLE HINSTANCE;
typedef HANDLE HWND;

typedef unsigned int COLORREF;

typedef struct crtgfx_win_rect {
  LONG left;
  LONG top;
  LONG right;
  LONG bottom;
} RECT;

typedef struct crtgfx_win_point {
  LONG x;
  LONG y;
} POINT;

typedef LRESULT(CRTGFX_WINAPI* WNDPROC)(HWND, UINT, WPARAM, LPARAM);

typedef struct crtgfx_win_wndclassexa {
  UINT cbSize;
  UINT style;
  WNDPROC lpfnWndProc;
  int cbClsExtra;
  int cbWndExtra;
  HINSTANCE hInstance;
  HANDLE hIcon;
  HCURSOR hCursor;
  HBRUSH hbrBackground;
  const char* lpszMenuName;
  const char* lpszClassName;
  HANDLE hIconSm;
} WNDCLASSEXA;

typedef struct crtgfx_win_createstructa {
  void* lpCreateParams;
  HINSTANCE hInstance;
  HANDLE hMenu;
  HWND hwndParent;
  int cy;
  int cx;
  int y;
  int x;
  LONG style;
  const char* lpszName;
  const char* lpszClass;
  DWORD dwExStyle;
} CREATESTRUCTA;

typedef struct crtgfx_win_paintstruct {
  HDC hdc;
  BOOL fErase;
  RECT rcPaint;
  BOOL fRestore;
  BOOL fIncUpdate;
  unsigned char rgbReserved[32];
} PAINTSTRUCT;

typedef struct crtgfx_win_msg {
  HWND hwnd;
  UINT message;
  WPARAM wParam;
  LPARAM lParam;
  DWORD time;
  POINT pt;
  DWORD lPrivate;
} MSG;

typedef struct crtgfx_win_bitmapinfoheader {
  DWORD biSize;
  LONG biWidth;
  LONG biHeight;
  WORD biPlanes;
  WORD biBitCount;
  DWORD biCompression;
  DWORD biSizeImage;
  LONG biXPelsPerMeter;
  LONG biYPelsPerMeter;
  DWORD biClrUsed;
  DWORD biClrImportant;
} BITMAPINFOHEADER;

typedef struct crtgfx_win_rgbquad {
  unsigned char rgbBlue;
  unsigned char rgbGreen;
  unsigned char rgbRed;
  unsigned char rgbReserved;
} RGBQUAD;

typedef struct crtgfx_win_bitmapinfo {
  BITMAPINFOHEADER bmiHeader;
  RGBQUAD bmiColors[1];
} BITMAPINFO;

#define FALSE 0
#define HEAP_ZERO_MEMORY 0x00000008u
#define WM_DESTROY 0x0002u
#define WM_SIZE 0x0005u
#define WM_SETFOCUS 0x0007u
#define WM_KILLFOCUS 0x0008u
#define WM_CLOSE 0x0010u
#define WM_PAINT 0x000fu
#define WM_NCCREATE 0x0081u
#define WM_KEYDOWN 0x0100u
#define WM_KEYUP 0x0101u
#define WM_CHAR 0x0102u
#define WM_SYSKEYDOWN 0x0104u
#define WM_SYSKEYUP 0x0105u
#define WM_MOUSEMOVE 0x0200u
#define WM_LBUTTONDOWN 0x0201u
#define WM_LBUTTONUP 0x0202u
#define WM_RBUTTONDOWN 0x0204u
#define WM_RBUTTONUP 0x0205u
#define WM_MBUTTONDOWN 0x0207u
#define WM_MBUTTONUP 0x0208u
#define WM_MOUSEWHEEL 0x020au
#define WM_MOUSEHWHEEL 0x020eu
/* One wheel "notch" (real, standard Win32 constant, unchanged since the
 * original Win32 API). */
#define WHEEL_DELTA 120
/* Real, standard Win32 constant (winuser.h) -- confirmed directly
 * against this machine's own real Windows 10 SDK (10.0.28000.0), not
 * assumed. */
#define WM_DPICHANGED 0x02e0u
#define GWLP_USERDATA (-21)
#define COLOR_WINDOW 5
#define WS_OVERLAPPEDWINDOW 0x00cf0000u
#define CW_USEDEFAULT ((int)0x80000000u)
#define PM_REMOVE 0x0001u
#define SW_SHOWNORMAL 1
#define IDC_ARROW ((const char*)(uintptr_t)32512u)
#define BI_RGB 0u
#define DIB_RGB_COLORS 0u
#define SRCCOPY 0x00cc0020u
/* Real, standard Win32 constants (winuser.h), confirmed against this
 * machine's own real SDK -- used by crtgfx_window_proc()'s own
 * WM_DPICHANGED handling to reposition/resize the window to the rect
 * Windows itself suggests for the new DPI. */
#define SWP_NOZORDER 0x0004u
#define SWP_NOACTIVATE 0x0010u
/* DPI_AWARENESS_CONTEXT is a real opaque HANDLE-shaped type
 * (DECLARE_HANDLE in the real SDK's windef.h) -- void* here matches
 * this file's own established HANDLE-as-void* convention. The V2
 * sentinel value (real, documented, confirmed directly against this
 * machine's own real SDK's windef.h, not assumed) is a small negative
 * integer cast to that pointer type, not a real pointer. */
typedef void* DPI_AWARENESS_CONTEXT;
#define CRTGFX_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)(intptr_t)-4)

#define LOWORD(value) ((WORD)((uintptr_t)(value) & 0xffffu))
#define HIWORD(value) ((WORD)(((uintptr_t)(value) >> 16) & 0xffffu))
/* signed low/high 16 bits -- needed for WM_MOUSEMOVE/button lParam,
 * which packs client-area coordinates that can legitimately be negative
 * on a multi-monitor setup with a monitor to the left/above the primary
 * one (LOWORD/HIWORD above are unsigned and would corrupt those). */
#define GET_X_LPARAM(lparam) ((int)(short)LOWORD(lparam))
#define GET_Y_LPARAM(lparam) ((int)(short)HIWORD(lparam))
/* WM_MOUSEWHEEL/WM_MOUSEHWHEEL's own wParam packs the wheel rotation
 * amount (signed, WHEEL_DELTA-scaled) in its high word and key-state
 * flags (MK_CONTROL/...) in its low word -- same signed-HIWORD shape as
 * GET_Y_LPARAM above, real standard Win32 macro, not this file's own
 * invention. */
#define GET_WHEEL_DELTA_WPARAM(wparam) ((int)(short)HIWORD(wparam))

/* Real, standard Win32 virtual-key codes (winuser.h, unchanged since the
 * original Win32 API -- these are stable ABI constants, not guessed).
 * Only the ones this file's own VK_TO_EVDEV table (below) needs. */
#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_RETURN 0x0d
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define VK_PAUSE 0x13
#define VK_CAPITAL 0x14
#define VK_ESCAPE 0x1b
#define VK_SPACE 0x20
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_INSERT 0x2d
#define VK_DELETE 0x2e
#define VK_LWIN 0x5b
#define VK_RWIN 0x5c
#define VK_NUMPAD0 0x60
#define VK_MULTIPLY 0x6a
#define VK_ADD 0x6b
#define VK_SUBTRACT 0x6d
#define VK_DECIMAL 0x6e
#define VK_DIVIDE 0x6f
#define VK_F1 0x70
#define VK_NUMLOCK 0x90
#define VK_SCROLL 0x91
#define VK_OEM_1 0xba
#define VK_OEM_PLUS 0xbb
#define VK_OEM_COMMA 0xbc
#define VK_OEM_MINUS 0xbd
#define VK_OEM_PERIOD 0xbe
#define VK_OEM_2 0xbf
#define VK_OEM_3 0xc0
#define VK_OEM_4 0xdb
#define VK_OEM_5 0xdc
#define VK_OEM_6 0xdd
#define VK_OEM_7 0xde

__declspec(dllimport) short CRTGFX_WINAPI GetKeyState(int nVirtKey);
__declspec(dllimport) HANDLE CRTGFX_WINAPI GetProcessHeap(void);
__declspec(dllimport) void* CRTGFX_WINAPI HeapAlloc(HANDLE hHeap, DWORD dwFlags, size_t dwBytes);
__declspec(dllimport) BOOL CRTGFX_WINAPI HeapFree(HANDLE hHeap, DWORD dwFlags, void* lpMem);
__declspec(dllimport) HINSTANCE CRTGFX_WINAPI GetModuleHandleA(const char* lpModuleName);
__declspec(dllimport) void CRTGFX_WINAPI Sleep(DWORD dwMilliseconds);
__declspec(dllimport) DWORD CRTGFX_WINAPI GetTickCount(void);

__declspec(dllimport) ATOM CRTGFX_WINAPI RegisterClassExA(const WNDCLASSEXA* lpWndClass);
__declspec(dllimport) HWND CRTGFX_WINAPI CreateWindowExA(
    DWORD dwExStyle, const char* lpClassName, const char* lpWindowName, DWORD dwStyle, int X, int Y,
    int nWidth, int nHeight, HWND hWndParent, HANDLE hMenu, HINSTANCE hInstance, void* lpParam);
__declspec(dllimport) BOOL CRTGFX_WINAPI DestroyWindow(HWND hWnd);
__declspec(dllimport) BOOL CRTGFX_WINAPI ShowWindow(HWND hWnd, int nCmdShow);
__declspec(dllimport) BOOL CRTGFX_WINAPI UpdateWindow(HWND hWnd);
__declspec(dllimport) BOOL CRTGFX_WINAPI AdjustWindowRectEx(
    RECT* lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle);
__declspec(dllimport) BOOL CRTGFX_WINAPI GetClientRect(HWND hWnd, RECT* lpRect);
__declspec(dllimport) BOOL CRTGFX_WINAPI SetWindowPos(
    HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags);
/* Real Win32 signatures, confirmed against this machine's own real SDK
 * (10.0.28000.0) -- both exported from user32, same as every other
 * dllimport in this file. SetProcessDpiAwarenessContext() needs calling
 * exactly once, before any window is created (see crtgfx_register_
 * window_class()'s own call below); without it, Windows falls back to
 * bitmap-stretching the whole window on a non-96-DPI monitor instead of
 * delivering real per-monitor DPI information at all, and WM_DPICHANGED
 * (crtgfx_window_proc()'s own handling below) would not fire correctly. */
__declspec(dllimport) BOOL CRTGFX_WINAPI SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT value);
__declspec(dllimport) UINT CRTGFX_WINAPI GetDpiForWindow(HWND hwnd);
__declspec(dllimport) HDC CRTGFX_WINAPI GetDC(HWND hWnd);
__declspec(dllimport) int CRTGFX_WINAPI ReleaseDC(HWND hWnd, HDC hDC);
__declspec(dllimport) LONG_PTR CRTGFX_WINAPI SetWindowLongPtrA(HWND hWnd, int nIndex, LONG_PTR dwNewLong);
__declspec(dllimport) LONG_PTR CRTGFX_WINAPI GetWindowLongPtrA(HWND hWnd, int nIndex);
__declspec(dllimport) LRESULT CRTGFX_WINAPI DefWindowProcA(
    HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
__declspec(dllimport) HCURSOR CRTGFX_WINAPI LoadCursorA(HINSTANCE hInstance, const char* lpCursorName);
__declspec(dllimport) HDC CRTGFX_WINAPI BeginPaint(HWND hWnd, PAINTSTRUCT* lpPaint);
__declspec(dllimport) BOOL CRTGFX_WINAPI EndPaint(HWND hWnd, const PAINTSTRUCT* lpPaint);
__declspec(dllimport) BOOL CRTGFX_WINAPI PeekMessageA(
    MSG* lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg);
__declspec(dllimport) BOOL CRTGFX_WINAPI TranslateMessage(const MSG* lpMsg);
__declspec(dllimport) LRESULT CRTGFX_WINAPI DispatchMessageA(const MSG* lpMsg);
__declspec(dllimport) int CRTGFX_WINAPI StretchDIBits(
    HDC hdc, int xDest, int yDest, int DestWidth, int DestHeight, int xSrc, int ySrc, int SrcWidth,
    int SrcHeight, const void* lpBits, const BITMAPINFO* lpbmi, UINT iUsage, DWORD rop);
/* WaitForSingleObject(..., 0): a real, standard non-blocking poll (a
 * 0-millisecond timeout returns immediately either way -- WAIT_OBJECT_0
 * if already signaled, WAIT_TIMEOUT otherwise), the same technique this
 * file's own crtgfx_host_window_dispatch() and crtgfx_host_window_
 * present_software() below use to check a swap chain's own frame-latency
 * waitable without ever blocking the caller's single thread. Confirmed
 * against dxgi_probe.c's own real, working WaitForSingleObjectEx() call
 * (this file only needs the simpler non-alertable variant). */
__declspec(dllimport) DWORD CRTGFX_WINAPI WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
__declspec(dllimport) BOOL CRTGFX_WINAPI CloseHandle(HANDLE hObject);
#define WAIT_OBJECT_0 0x00000000ul

/* ---- Minimal DXGI/D3D11 COM surface, added 2026-08-30 for a genuinely
 * asynchronous CRTGFX_EVENT_FRAME_COMPLETE on Windows ----
 *
 * Why this exists at all: empirically confirmed on this real machine
 * (dwm_probe.c/dwm_probe2.c, not this project's own source -- see
 * HISTORY.md) that plain GDI StretchDIBits presentation has NO real
 * per-window completion signal to poll -- DwmGetCompositionTimingInfo()
 * with a real GDI window's own HWND fails outright (E_INVALIDARG, every
 * call, on this real Windows 11 machine); it only "works" with hwnd=NULL,
 * which is the desktop's own global compositing heartbeat, advancing
 * every call regardless of whether this process ever draws anything at
 * all -- not a signal tied to any specific window's own submitted
 * content. A real per-buffer completion signal on Windows genuinely
 * requires a DXGI flip-model swap chain (IDXGISwapChain2::
 * GetFrameLatencyWaitableObject(), a real per-swap-chain kernel event the
 * OS signals once that swap chain's own previous buffer has actually been
 * retired) -- the Windows analog of Linux's wl_surface::frame and macOS's
 * CATransaction completion block. This is why crtgfx_host_window_present_
 * software() below no longer uses StretchDIBits at all: GDI and DXGI are
 * two unrelated presentation pipelines, there is no way to keep painting
 * through GDI while asking DXGI for a completion signal on content nobody
 * is compositing.
 *
 * Every vtable layout below is transcribed directly from this machine's
 * own real Windows 10 SDK (10.0.28000.0) headers -- shared/dxgi.h,
 * shared/dxgi1_2.h, shared/dxgi1_3.h, um/d3d11.h -- method by method, not
 * guessed: a COM vtable is a fixed binary ABI (an ordered array of
 * function pointers), where a wrong slot count silently calls the wrong
 * function through a stale pointer rather than failing to compile, unlike
 * this file's own existing flat user32/gdi32 dllimports (which only need
 * a matching *signature*, not a matching *position* in some larger
 * table). Confirmed for real before landing here: a standalone probe
 * (dxgi_probe.c, real d3d11.h/dxgi1_3.h SDK headers, not this file's own
 * hand-rolled ones) built and ran this exact sequence -- device creation,
 * swap chain creation against a real HWND, UpdateSubresource, Present,
 * QueryInterface to IDXGISwapChain2, SetMaximumFrameLatency/
 * GetFrameLatencyWaitableObject, and polling the waitable -- on this real
 * machine, confirming every GUID/vtable-slot transcription below against
 * the real, working thing before hand-declaring the equivalent here. Each
 * interface is declared only up to and including the last method this
 * file actually calls; the untouched real methods before it are
 * represented as anonymous `void* reserved[N]` slots -- only their
 * *count* matters for correct layout, not their individual real types,
 * since every vtable slot is a same-size function pointer regardless of
 * signature, and this file never calls through a reserved slot. */

typedef long HRESULT;
typedef unsigned long ULONG;
typedef struct crtgfx_dxgi_guid {
  uint32_t Data1;
  uint16_t Data2;
  uint16_t Data3;
  unsigned char Data4[8];
} GUID;
typedef GUID IID;
typedef const GUID* REFIID;
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)

/* Real IIDs (this machine's own real SDK headers' own MIDL_INTERFACE(...)
 * UUID strings, mechanically converted to their GUID struct form --
 * confirmed directly against um/d3d11.h/shared/dxgi1_2.h/dxgi1_3.h, not
 * guessed, and confirmed working via dxgi_probe.c's own real, successful
 * QueryInterface/GetBuffer calls using these exact same values). */
static const GUID crtgfx_iid_idxgi_factory2 = {
    0x50c83a1c, 0xe072, 0x4c48, {0x87, 0xb0, 0x36, 0x30, 0xfa, 0x36, 0xa6, 0xd0}};
static const GUID crtgfx_iid_id3d11_texture2d = {
    0x6f15aaf2, 0xd208, 0x4e89, {0x9a, 0xb4, 0x48, 0x95, 0x35, 0xd3, 0x4f, 0x9c}};
static const GUID crtgfx_iid_idxgi_swapchain2 = {
    0xa8be2ac4, 0x199f, 0x4946, {0xb3, 0x31, 0x79, 0x59, 0x9f, 0xb9, 0x8d, 0xe7}};

typedef struct crtgfx_dxgi_unknown crtgfx_dxgi_unknown;
typedef struct crtgfx_dxgi_unknown_vtbl {
  HRESULT(CRTGFX_WINAPI* QueryInterface)(crtgfx_dxgi_unknown* self, REFIID riid, void** out);
  ULONG(CRTGFX_WINAPI* AddRef)(crtgfx_dxgi_unknown* self);
  ULONG(CRTGFX_WINAPI* Release)(crtgfx_dxgi_unknown* self);
} crtgfx_dxgi_unknown_vtbl;
struct crtgfx_dxgi_unknown {
  const crtgfx_dxgi_unknown_vtbl* lpVtbl;
};
/* Used for D3D11CreateDevice()'s own ID3D11Device** out-param (this file
 * never calls anything on it beyond Release -- swap chain creation only
 * needs it as an opaque IUnknown*) and for ID3D11Texture2D (this file
 * only ever passes it straight into UpdateSubresource/Release, never
 * calls a texture-specific method on it). */
typedef crtgfx_dxgi_unknown crtgfx_d3d11_device;
typedef crtgfx_dxgi_unknown crtgfx_d3d11_texture2d;

typedef struct crtgfx_d3d11_box {
  UINT left;
  UINT top;
  UINT front;
  UINT right;
  UINT bottom;
  UINT back;
} crtgfx_d3d11_box;

typedef struct crtgfx_d3d11_device_context crtgfx_d3d11_device_context;
typedef struct crtgfx_d3d11_device_context_vtbl {
  HRESULT(CRTGFX_WINAPI* QueryInterface)(crtgfx_d3d11_device_context* self, REFIID riid, void** out);
  ULONG(CRTGFX_WINAPI* AddRef)(crtgfx_d3d11_device_context* self);
  ULONG(CRTGFX_WINAPI* Release)(crtgfx_d3d11_device_context* self);
  /* Real methods GetDevice through CopyResource -- um/d3d11.h's own
   * ID3D11DeviceContextVtbl, vtable slots 3-47 inclusive (45 methods,
   * confirmed by direct line count against that real declaration). Never
   * called here; only the count matters, so UpdateSubresource just below
   * lands at its own real slot 48. */
  void* reserved_3_to_47[45];
  void(CRTGFX_WINAPI* UpdateSubresource)(
      crtgfx_d3d11_device_context* self, crtgfx_dxgi_unknown* dst_resource, UINT dst_subresource,
      const crtgfx_d3d11_box* dst_box, const void* src_data, UINT src_row_pitch,
      UINT src_depth_pitch);
} crtgfx_d3d11_device_context_vtbl;
struct crtgfx_d3d11_device_context {
  const crtgfx_d3d11_device_context_vtbl* lpVtbl;
};

typedef struct crtgfx_dxgi_factory2 crtgfx_dxgi_factory2;
typedef struct crtgfx_dxgi_factory2_vtbl {
  HRESULT(CRTGFX_WINAPI* QueryInterface)(crtgfx_dxgi_factory2* self, REFIID riid, void** out);
  ULONG(CRTGFX_WINAPI* AddRef)(crtgfx_dxgi_factory2* self);
  ULONG(CRTGFX_WINAPI* Release)(crtgfx_dxgi_factory2* self);
  /* SetPrivateData through IsWindowedStereoEnabled -- shared/dxgi1_2.h's
   * own IDXGIFactory2Vtbl, slots 3-14 inclusive (12 methods). */
  void* reserved_3_to_14[12];
  HRESULT(CRTGFX_WINAPI* CreateSwapChainForHwnd)(
      crtgfx_dxgi_factory2* self, crtgfx_dxgi_unknown* device, HWND hwnd, const void* desc,
      const void* fullscreen_desc, void* restrict_to_output, void** out_swap_chain);
} crtgfx_dxgi_factory2_vtbl;
struct crtgfx_dxgi_factory2 {
  const crtgfx_dxgi_factory2_vtbl* lpVtbl;
};

typedef struct crtgfx_dxgi_swapchain1 crtgfx_dxgi_swapchain1;
typedef struct crtgfx_dxgi_swapchain1_vtbl {
  HRESULT(CRTGFX_WINAPI* QueryInterface)(crtgfx_dxgi_swapchain1* self, REFIID riid, void** out);
  ULONG(CRTGFX_WINAPI* AddRef)(crtgfx_dxgi_swapchain1* self);
  ULONG(CRTGFX_WINAPI* Release)(crtgfx_dxgi_swapchain1* self);
  /* SetPrivateData through GetDevice -- shared/dxgi.h's own
   * IDXGISwapChainVtbl (IDXGISwapChain1's own base), slots 3-7 (5
   * methods). */
  void* reserved_3_to_7[5];
  HRESULT(CRTGFX_WINAPI* Present)(crtgfx_dxgi_swapchain1* self, UINT sync_interval, UINT flags);
  HRESULT(CRTGFX_WINAPI* GetBuffer)(
      crtgfx_dxgi_swapchain1* self, UINT buffer, REFIID riid, void** out);
  /* SetFullscreenState through GetDesc -- slots 10-12 (3 methods). */
  void* reserved_10_to_12[3];
  HRESULT(CRTGFX_WINAPI* ResizeBuffers)(
      crtgfx_dxgi_swapchain1* self, UINT buffer_count, UINT width, UINT height, UINT new_format,
      UINT flags);
} crtgfx_dxgi_swapchain1_vtbl;
struct crtgfx_dxgi_swapchain1 {
  const crtgfx_dxgi_swapchain1_vtbl* lpVtbl;
};

typedef struct crtgfx_dxgi_swapchain2 crtgfx_dxgi_swapchain2;
typedef struct crtgfx_dxgi_swapchain2_vtbl {
  HRESULT(CRTGFX_WINAPI* QueryInterface)(crtgfx_dxgi_swapchain2* self, REFIID riid, void** out);
  ULONG(CRTGFX_WINAPI* AddRef)(crtgfx_dxgi_swapchain2* self);
  ULONG(CRTGFX_WINAPI* Release)(crtgfx_dxgi_swapchain2* self);
  /* SetPrivateData through GetSourceSize -- shared/dxgi1_3.h's own
   * IDXGISwapChain2Vtbl, slots 3-30 inclusive (28 methods, everything
   * IDXGISwapChain1 already has plus SetSourceSize/GetSourceSize). */
  void* reserved_3_to_30[28];
  HRESULT(CRTGFX_WINAPI* SetMaximumFrameLatency)(crtgfx_dxgi_swapchain2* self, UINT max_latency);
  /* GetMaximumFrameLatency -- slot 32 (1 method). */
  void* reserved_32[1];
  HANDLE(CRTGFX_WINAPI* GetFrameLatencyWaitableObject)(crtgfx_dxgi_swapchain2* self);
} crtgfx_dxgi_swapchain2_vtbl;
struct crtgfx_dxgi_swapchain2 {
  const crtgfx_dxgi_swapchain2_vtbl* lpVtbl;
};

typedef struct crtgfx_dxgi_sample_desc {
  UINT Count;
  UINT Quality;
} crtgfx_dxgi_sample_desc;

/* shared/dxgi1_2.h's own real DXGI_SWAP_CHAIN_DESC1 field order/types. */
typedef struct crtgfx_dxgi_swap_chain_desc1 {
  UINT Width;
  UINT Height;
  UINT Format;
  BOOL Stereo;
  crtgfx_dxgi_sample_desc SampleDesc;
  UINT BufferUsage;
  UINT BufferCount;
  UINT Scaling;
  UINT SwapEffect;
  UINT AlphaMode;
  UINT Flags;
} crtgfx_dxgi_swap_chain_desc1;

/* Real, standard DXGI/D3D11 constants (confirmed against this machine's
 * own real SDK headers, not guessed). */
#define CRTGFX_DXGI_FORMAT_B8G8R8A8_UNORM 87u
#define CRTGFX_DXGI_USAGE_RENDER_TARGET_OUTPUT 0x00000020ul
#define CRTGFX_DXGI_SCALING_STRETCH 0u
#define CRTGFX_DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL 3u
#define CRTGFX_DXGI_ALPHA_MODE_IGNORE 3u
#define CRTGFX_DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT 64u
#define CRTGFX_D3D_DRIVER_TYPE_HARDWARE 1
#define CRTGFX_D3D_DRIVER_TYPE_WARP 5
#define CRTGFX_D3D_FEATURE_LEVEL_11_0 0xb000

__declspec(dllimport) HRESULT CRTGFX_WINAPI CreateDXGIFactory1(REFIID riid, void** out_factory);
/* Real signature (um/d3d11.h's own PFN_D3D11_CREATE_DEVICE) with every
 * host-SDK-specific pointer type this file has no other need to declare
 * (IDXGIAdapter*, D3D_FEATURE_LEVEL*) narrowed to a plain void pointer or
 * int instead -- safe here since this file always passes 0/NULL for the
 * adapter and a single literal feature level, never reads either type's
 * own real fields. */
__declspec(dllimport) HRESULT CRTGFX_WINAPI D3D11CreateDevice(
    void* adapter, int driver_type, HANDLE software, UINT flags, const int* feature_levels,
    UINT num_feature_levels, UINT sdk_version, crtgfx_d3d11_device** out_device,
    int* out_feature_level, crtgfx_d3d11_device_context** out_context);

struct crtgfx_host_window {
  HWND hwnd;
  crtgfx_weston_toplevel* toplevel;
  /* WM_CHAR delivers UTF-16 one code unit at a time; a character outside
   * the BMP (rare -- e.g. an emoji) arrives as a surrogate *pair* across
   * two separate WM_CHAR messages, so the high surrogate has to be held
   * here until its matching low surrogate shows up. 0 = none pending. */
  uint16_t pending_high_surrogate;

  /* Real DXGI/D3D11 presentation state, added 2026-08-30 for a genuinely
   * asynchronous CRTGFX_EVENT_FRAME_COMPLETE -- see this file's own top
   * comment on the crtgfx_dxgi_ and crtgfx_d3d11_ declarations for why
   * GDI/StretchDIBits could never provide this. d3d_device/d3d_context are
   * created once per window (D3D11 objects are cheap and this project
   * has no reason yet to share one device across windows the way Linux's
   * single wl_display connection is shared -- see window_wayland.c's own
   * comment on that decision). swap_chain2 is swap_chain's own
   * IDXGISwapChain2 identity (same object, QueryInterface'd once at
   * creation) -- kept as a second pointer purely so this file never needs
   * to re-QueryInterface on every present. */
  crtgfx_d3d11_device* d3d_device;
  crtgfx_d3d11_device_context* d3d_context;
  crtgfx_dxgi_swapchain1* swap_chain;
  crtgfx_dxgi_swapchain2* swap_chain2;
  HANDLE frame_latency_waitable;
  /* The swap chain's own current buffer size -- tracked separately from
   * crtgfx_weston_toplevel::width/height (wayland_weston_internal.h)
   * purely to know whether a real ResizeBuffers() call is actually needed
   * on a given WM_SIZE (skipped when already correct, and always skipped
   * at 0x0 -- minimized -- since ResizeBuffers rejects a zero dimension). */
  uint32_t swap_chain_width;
  uint32_t swap_chain_height;
  /* Set after this window's own most recent Present() call, cleared once
   * that submission's own frame-latency waitable has been observed
   * signaled (crtgfx_host_window_dispatch() below, polled non-blocking,
   * same "at most one outstanding, checked on a later dispatch() call"
   * shape as window_wayland.c's own frame_callback_id). */
  int frame_complete_pending;
  /* crtgfx_win_windows linked-list link -- needed so crtgfx_host_window_
   * dispatch() (which, unlike present_software(), has no specific host
   * window handed to it) can poll every live window's own waitable once
   * per dispatch() call, the same reason Linux's conn->windows and
   * macOS's crtgfx_cocoa_windows lists exist. */
  struct crtgfx_host_window* next;
};

/* Every live crtgfx_host_window in this process -- see struct crtgfx_
 * host_window::next's own comment. Win32 window routing itself still
 * needs no such list (GWLP_USERDATA already resolves a specific HWND's
 * own host directly, unlike Linux/macOS's shared-connection/shared-
 * run-loop routing) -- this list exists purely for crtgfx_host_window_
 * dispatch()'s own frame-latency-waitable polling, added 2026-08-30. */
static struct crtgfx_host_window* crtgfx_win_windows;

static const char crtgfx_window_class_name[] = "crtgfx_window";
static ATOM crtgfx_window_class_atom;

/* Real, standard Linux evdev keycodes (linux/input-event-codes.h, a
 * stable kernel uapi -- confirmed against a real copy of that header,
 * not guessed) matching crtgfx/window.h's own documented design: every
 * host backend normalizes its own native keycode onto this one evdev
 * numbering, since it's the first backend (Wayland, see
 * src/arch/linux/window_wayland.c) to actually receive keyboard input at
 * all. Simplification, documented rather than silently assumed: Windows
 * generic WM_KEYDOWN delivers VK_SHIFT/VK_CONTROL/VK_MENU (not the L/R-
 * specific VK_LSHIFT/VK_RSHIFT/... variants) unless the caller goes out
 * of its way to disambiguate via the scan code in lParam -- this table
 * always maps to the *left* evdev variant (KEY_LEFTSHIFT/KEY_LEFTCTRL/
 * KEY_LEFTALT) regardless of which physical key was actually pressed.
 * crtgfx_event's own modifiers bitmask (CRTGFX_MOD_SHIFT/CTRL/ALT/SUPER,
 * populated below via GetKeyState()) is what a real app should use to
 * ask "is shift/ctrl/alt down", not the raw keycode -- left/right
 * disambiguation only matters for an app that cares which physical key
 * was pressed, which crtgfx_event's own design doesn't promise anywhere. */
static uint32_t crtgfx_win_vk_to_evdev(unsigned int vk, unsigned int scancode, int is_extended) {
  switch (vk) {
    case VK_ESCAPE: return 1u;
    case '1': return 2u;
    case '2': return 3u;
    case '3': return 4u;
    case '4': return 5u;
    case '5': return 6u;
    case '6': return 7u;
    case '7': return 8u;
    case '8': return 9u;
    case '9': return 10u;
    case '0': return 11u;
    case VK_OEM_MINUS: return 12u;
    case VK_OEM_PLUS: return 13u;
    case VK_BACK: return 14u;
    case VK_TAB: return 15u;
    case 'Q': return 16u;
    case 'W': return 17u;
    case 'E': return 18u;
    case 'R': return 19u;
    case 'T': return 20u;
    case 'Y': return 21u;
    case 'U': return 22u;
    case 'I': return 23u;
    case 'O': return 24u;
    case 'P': return 25u;
    case VK_OEM_4: return 26u;  /* [ { */
    case VK_OEM_6: return 27u;  /* ] } */
    case VK_RETURN: return is_extended ? 96u : 28u; /* numpad Enter is "extended" */
    case VK_CONTROL: return is_extended ? 97u : 29u; /* right Ctrl is "extended" */
    case 'A': return 30u;
    case 'S': return 31u;
    case 'D': return 32u;
    case 'F': return 33u;
    case 'G': return 34u;
    case 'H': return 35u;
    case 'J': return 36u;
    case 'K': return 37u;
    case 'L': return 38u;
    case VK_OEM_1: return 39u;  /* ; : */
    case VK_OEM_7: return 40u;  /* ' " */
    case VK_OEM_3: return 41u;  /* ` ~ */
    case VK_SHIFT:
      /* Generic VK_SHIFT can't tell left/right apart from vk alone;
       * scancode 0x36 is the real, standard PS/2 Set-1 scan code for the
       * right Shift key (left Shift is 0x2a) -- distinguishable this
       * way, unlike Ctrl/Alt, which Windows already flags via the
       * "extended key" lParam bit instead. */
      return (scancode == 0x36u) ? 54u : 42u;
    case VK_OEM_5: return 43u;  /* \ | */
    case 'Z': return 44u;
    case 'X': return 45u;
    case 'C': return 46u;
    case 'V': return 47u;
    case 'B': return 48u;
    case 'N': return 49u;
    case 'M': return 50u;
    case VK_OEM_COMMA: return 51u;
    case VK_OEM_PERIOD: return 52u;
    case VK_OEM_2: return 53u;  /* / ? */
    case VK_MULTIPLY: return 55u;
    case VK_MENU: return is_extended ? 100u : 56u; /* right Alt is "extended" */
    case VK_SPACE: return 57u;
    case VK_CAPITAL: return 58u;
    case VK_F1: return 59u;
    case VK_F1 + 1: return 60u;
    case VK_F1 + 2: return 61u;
    case VK_F1 + 3: return 62u;
    case VK_F1 + 4: return 63u;
    case VK_F1 + 5: return 64u;
    case VK_F1 + 6: return 65u;
    case VK_F1 + 7: return 66u;
    case VK_F1 + 8: return 67u;
    case VK_F1 + 9: return 68u;
    case VK_NUMLOCK: return 69u;
    case VK_SCROLL: return 70u;
    case VK_NUMPAD0 + 7: return 71u;
    case VK_NUMPAD0 + 8: return 72u;
    case VK_NUMPAD0 + 9: return 73u;
    case VK_SUBTRACT: return 74u;
    case VK_NUMPAD0 + 4: return 75u;
    case VK_NUMPAD0 + 5: return 76u;
    case VK_NUMPAD0 + 6: return 77u;
    case VK_ADD: return 78u;
    case VK_NUMPAD0 + 1: return 79u;
    case VK_NUMPAD0 + 2: return 80u;
    case VK_NUMPAD0 + 3: return 81u;
    case VK_NUMPAD0: return 82u;
    case VK_DECIMAL: return 83u;
    case VK_F1 + 10: return 87u;  /* F11 */
    case VK_F1 + 11: return 88u;  /* F12 */
    case VK_DIVIDE: return 98u;
    case VK_PAUSE: return 119u;
    case VK_HOME: return 102u;
    case VK_UP: return 103u;
    case VK_PRIOR: return 104u;
    case VK_LEFT: return 105u;
    case VK_RIGHT: return 106u;
    case VK_END: return 107u;
    case VK_DOWN: return 108u;
    case VK_NEXT: return 109u;
    case VK_INSERT: return 110u;
    case VK_DELETE: return 111u;
    case VK_LWIN: return 125u;
    case VK_RWIN: return 126u;
    default: return 0u; /* unmapped: no KEY_DOWN/UP is queued for it (see caller) */
  }
}

/* CRTGFX_MOD_* is host-independent (crtgfx/window.h); this queries
 * Windows' own current key state via GetKeyState() (the standard,
 * correct way to read modifier state mid-message on Win32 -- reflects
 * state as of the message currently being processed, not a racy separate
 * poll) rather than tracking press/release ourselves. High bit (0x8000)
 * set means currently down. */
static uint32_t crtgfx_win_query_modifiers(void) {
  uint32_t mods = 0u;
  if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) {
    mods |= CRTGFX_MOD_SHIFT;
  }
  if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
    mods |= CRTGFX_MOD_CTRL;
  }
  if ((GetKeyState(VK_MENU) & 0x8000) != 0) {
    mods |= CRTGFX_MOD_ALT;
  }
  if ((GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0) {
    mods |= CRTGFX_MOD_SUPER;
  }
  return mods;
}

/* Encodes one Unicode codepoint (already reassembled from a UTF-16
 * surrogate pair if needed -- see WM_CHAR handling below) as UTF-8.
 * Returns the byte count written (1-4), matching crtgfx_event.data.
 * text.utf8's own documented "enough for any single grapheme" sizing. */
static size_t crtgfx_win_utf8_encode(uint32_t codepoint, char* out) {
  if (codepoint <= 0x7fu) {
    out[0] = (char)codepoint;
    return 1;
  }
  if (codepoint <= 0x7ffu) {
    out[0] = (char)(0xc0u | (codepoint >> 6));
    out[1] = (char)(0x80u | (codepoint & 0x3fu));
    return 2;
  }
  if (codepoint <= 0xffffu) {
    out[0] = (char)(0xe0u | (codepoint >> 12));
    out[1] = (char)(0x80u | ((codepoint >> 6) & 0x3fu));
    out[2] = (char)(0x80u | (codepoint & 0x3fu));
    return 3;
  }
  out[0] = (char)(0xf0u | (codepoint >> 18));
  out[1] = (char)(0x80u | ((codepoint >> 12) & 0x3fu));
  out[2] = (char)(0x80u | ((codepoint >> 6) & 0x3fu));
  out[3] = (char)(0x80u | (codepoint & 0x3fu));
  return 4;
}

/* Non-blocking check of `host`'s own frame-latency waitable (see struct
 * crtgfx_host_window::frame_complete_pending's own comment) -- fires
 * CRTGFX_EVENT_FRAME_COMPLETE and clears the pending flag if the previous
 * Present() has genuinely been retired, does nothing otherwise (not yet
 * retired, or nothing was pending in the first place). WaitForSingleObject
 * with a 0ms timeout is a real, standard non-blocking poll (returns
 * immediately either way), never blocks the caller's single thread --
 * confirmed against dxgi_probe.c's own real, working use of this exact
 * waitable. Called from both crtgfx_host_window_dispatch() (the normal,
 * once-per-pump-cycle path every window is polled from) and
 * crtgfx_host_window_present_software() (a best-effort extra check right
 * before a new Present(), in case present_software() is being called
 * faster than dispatch() ever runs in between). */
static void crtgfx_win_poll_frame_complete(struct crtgfx_host_window* host) {
  if (host == 0 || !host->frame_complete_pending || host->frame_latency_waitable == 0) {
    return;
  }
  if (WaitForSingleObject(host->frame_latency_waitable, 0) == WAIT_OBJECT_0) {
    crtgfx_event event = {0};

    event.type = CRTGFX_EVENT_FRAME_COMPLETE;
    crtgfx_weston_toplevel_note_event(host->toplevel, &event);
    host->frame_complete_pending = 0;
  }
}

/* Real DXGI rule (Microsoft's own documented ResizeBuffers() contract):
 * every outstanding reference to the swap chain's own buffers must be
 * released before calling it. This file already satisfies that by
 * construction -- crtgfx_host_window_present_software() below always
 * Release()s the ID3D11Texture2D* it gets from GetBuffer() before
 * returning, so no back-buffer reference is ever held between calls, and
 * this project's own single-thread-only API contract (crtgfx/window.h)
 * means WM_SIZE can never arrive re-entrantly during a present_software()
 * call on a different thread. width/height of 0 (a real, normal
 * consequence of minimizing the window) is intentionally skipped, not
 * passed through -- ResizeBuffers rejects a zero dimension outright, and
 * there is nothing useful to present while minimized anyway. */
static void crtgfx_win_resize_swap_chain(struct crtgfx_host_window* host, uint32_t width, uint32_t height) {
  HRESULT hr;

  if (host == 0 || host->swap_chain == 0 || width == 0 || height == 0) {
    return;
  }
  if (host->swap_chain_width == width && host->swap_chain_height == height) {
    return;
  }
  hr = host->swap_chain->lpVtbl->ResizeBuffers(
      host->swap_chain, 0 /* keep current buffer count */, width, height,
      CRTGFX_DXGI_FORMAT_B8G8R8A8_UNORM, CRTGFX_DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
  if (SUCCEEDED(hr)) {
    host->swap_chain_width = width;
    host->swap_chain_height = height;
  }
  /* A failed ResizeBuffers is left as a real, visible degradation (the
   * next present_software() call's own GetBuffer()/Present() will likely
   * fail too, surfacing as CRTGFX_ERROR_HOST there) rather than silently
   * ignored -- but WM_SIZE's own WndProc return value is not the right
   * place to report a Win32 API failure, matching every other WM_SIZE
   * side effect in this file (crtgfx_weston_toplevel_note_size() itself
   * has no failure path either). */
}

static LRESULT CRTGFX_WINAPI crtgfx_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  crtgfx_host_window* host;

  if (message == WM_NCCREATE) {
    CREATESTRUCTA* create = (CREATESTRUCTA*)lparam;
    host = (crtgfx_host_window*)create->lpCreateParams;
    SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)host);
    return DefWindowProcA(hwnd, message, wparam, lparam);
  }

  host = (crtgfx_host_window*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
  switch (message) {
    case WM_CLOSE:
      if (host != 0) {
        crtgfx_weston_toplevel_note_close(host->toplevel);
      }
      DestroyWindow(hwnd);
      return 0;
    case WM_DESTROY:
      if (host != 0) {
        host->hwnd = 0;
        crtgfx_weston_toplevel_note_close(host->toplevel);
      }
      return 0;
    case WM_SIZE:
      if (host != 0) {
        crtgfx_weston_toplevel_note_size(host->toplevel, (uint32_t)LOWORD(lparam),
                                         (uint32_t)HIWORD(lparam));
        crtgfx_win_resize_swap_chain(host, (uint32_t)LOWORD(lparam), (uint32_t)HIWORD(lparam));
      }
      return 0;
    case WM_SETFOCUS:
      if (host != 0) {
        crtgfx_weston_toplevel_note_focus(host->toplevel, 1);
      }
      return 0;
    case WM_KILLFOCUS:
      if (host != 0) {
        crtgfx_weston_toplevel_note_focus(host->toplevel, 0);
      }
      return 0;
    case WM_PAINT: {
      PAINTSTRUCT paint;
      BeginPaint(hwnd, &paint);
      EndPaint(hwnd, &paint);
      return 0;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP: {
      /* lParam bit layout (real, standard Win32 fact, unchanged since
       * the original Win32 API): bits 16-23 = scan code, bit 24 =
       * extended-key flag (set for right Ctrl/Alt, the arrow-key
       * cluster, numpad Enter, ...) -- both needed by
       * crtgfx_win_vk_to_evdev() above to disambiguate left/right Ctrl/
       * Alt/numpad-Enter and (via the scan code specifically) left/right
       * Shift, none of which the raw VK_* code alone distinguishes. */
      unsigned int scancode = (unsigned int)((lparam >> 16) & 0xffu);
      int extended = (int)((lparam >> 24) & 1u);
      uint32_t keycode = crtgfx_win_vk_to_evdev((unsigned int)wparam, scancode, extended);

      if (host != 0 && keycode != 0u) {
        crtgfx_event event = {0};
        event.type = (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) ? CRTGFX_EVENT_KEY_DOWN
                                                                          : CRTGFX_EVENT_KEY_UP;
        event.data.key.keycode = keycode;
        event.data.key.modifiers = crtgfx_win_query_modifiers();
        crtgfx_weston_toplevel_note_event(host->toplevel, &event);
      }
      /* Always fall through to DefWindowProcA, not `return 0` -- required
       * for WM_SYSKEYDOWN/UP specifically (Alt+F4, Alt+Space system menu,
       * bare-Alt menu-focus toggle are all implemented *inside*
       * DefWindowProcA itself; swallowing the message here would break
       * those well-known system behaviors). Harmless for plain WM_KEYDOWN/
       * UP either way -- TranslateMessage() (which turns this same raw
       * WM_KEYDOWN into a later WM_CHAR) runs in the message loop itself
       * (crtgfx_host_window_dispatch(), before DispatchMessageA even
       * calls this WndProc), so it does not depend on what gets returned
       * here. */
      return DefWindowProcA(hwnd, message, wparam, lparam);
    }
    case WM_CHAR: {
      /* wParam is one UTF-16 code unit at a time (TranslateMessage()'s
       * own real, documented behavior) -- a codepoint outside the BMP
       * (e.g. an emoji) arrives as a surrogate *pair* across two
       * consecutive WM_CHAR messages, reassembled here via
       * host->pending_high_surrogate. Real, standard UTF-16 surrogate
       * ranges: high 0xd800-0xdbff, low 0xdc00-0xdfff (Unicode spec,
       * unchanged since UTF-16 was defined). */
      uint16_t unit = (uint16_t)wparam;
      uint32_t codepoint;

      if (host == 0) {
        return 0;
      }
      if (unit >= 0xd800u && unit <= 0xdbffu) {
        host->pending_high_surrogate = unit;
        return 0;
      }
      if (unit >= 0xdc00u && unit <= 0xdfffu) {
        if (host->pending_high_surrogate == 0u) {
          return 0; /* stray low surrogate with no preceding high one -- drop, not crash */
        }
        codepoint = 0x10000u + (((uint32_t)host->pending_high_surrogate - 0xd800u) << 10) +
                    ((uint32_t)unit - 0xdc00u);
        host->pending_high_surrogate = 0u;
      } else {
        codepoint = unit;
        host->pending_high_surrogate = 0u; /* clear any stale unpaired high surrogate */
      }
      /* Skip ASCII control codes (0x00-0x1f): WM_CHAR still fires for
       * them (Enter=0x0d, Tab=0x09, Backspace=0x08, Escape=0x1b, Ctrl+A=
       * 0x01, ...), but those are already fully reported as real
       * KEY_DOWN/KEY_UP events above -- surfacing e.g. Backspace *again*
       * here as a one-byte "text" event would be a real duplicate/
       * misrepresentation (a Backspace keypress does not mean "insert
       * the character 0x08" to any real text editor). */
      if (codepoint >= 0x20u) {
        crtgfx_event event = {0};
        event.type = CRTGFX_EVENT_TEXT;
        crtgfx_win_utf8_encode(codepoint, event.data.text.utf8);
        crtgfx_weston_toplevel_note_event(host->toplevel, &event);
      }
      return 0;
    }
    case WM_MOUSEMOVE:
      if (host != 0) {
        crtgfx_event event = {0};
        event.type = CRTGFX_EVENT_POINTER_MOTION;
        event.data.pointer_motion.x = (double)GET_X_LPARAM(lparam);
        event.data.pointer_motion.y = (double)GET_Y_LPARAM(lparam);
        crtgfx_weston_toplevel_note_event(host->toplevel, &event);
      }
      return 0;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
      if (host != 0) {
        crtgfx_event event = {0};
        event.type = CRTGFX_EVENT_POINTER_BUTTON_DOWN;
        event.data.pointer_button.button = (message == WM_LBUTTONDOWN)   ? CRTGFX_POINTER_BUTTON_LEFT
                                            : (message == WM_RBUTTONDOWN) ? CRTGFX_POINTER_BUTTON_RIGHT
                                                                          : CRTGFX_POINTER_BUTTON_MIDDLE;
        event.data.pointer_button.x = (double)GET_X_LPARAM(lparam);
        event.data.pointer_button.y = (double)GET_Y_LPARAM(lparam);
        crtgfx_weston_toplevel_note_event(host->toplevel, &event);
      }
      return 0;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
      if (host != 0) {
        crtgfx_event event = {0};
        event.type = CRTGFX_EVENT_POINTER_BUTTON_UP;
        event.data.pointer_button.button = (message == WM_LBUTTONUP)   ? CRTGFX_POINTER_BUTTON_LEFT
                                            : (message == WM_RBUTTONUP) ? CRTGFX_POINTER_BUTTON_RIGHT
                                                                        : CRTGFX_POINTER_BUTTON_MIDDLE;
        event.data.pointer_button.x = (double)GET_X_LPARAM(lparam);
        event.data.pointer_button.y = (double)GET_Y_LPARAM(lparam);
        crtgfx_weston_toplevel_note_event(host->toplevel, &event);
      }
      return 0;
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
      /* Sign/scale follow the wire value directly (see crtgfx/window.h's
       * own CRTGFX_EVENT_POINTER_SCROLL doc comment) -- expressed here as
       * fractional WHEEL_DELTA "notches", not re-normalized against
       * Linux's own wl_pointer::axis convention. Real Win32 fact, not
       * this file's own choice: unlike every other mouse message here,
       * WM_MOUSEWHEEL/WM_MOUSEHWHEEL are sent to the currently *focused*
       * window regardless of cursor position (or posted to the thread
       * queue with hwnd looked up via WindowFromPoint() by the OS itself
       * before this WndProc ever sees it) -- host is therefore not
       * necessarily the window the cursor is physically over, matching
       * every other native Windows application's own scroll behavior. */
      if (host != 0) {
        crtgfx_event event = {0};
        double notches = (double)GET_WHEEL_DELTA_WPARAM(wparam) / (double)WHEEL_DELTA;

        event.type = CRTGFX_EVENT_POINTER_SCROLL;
        if (message == WM_MOUSEWHEEL) {
          event.data.pointer_scroll.dy = notches;
        } else {
          event.data.pointer_scroll.dx = notches;
        }
        crtgfx_weston_toplevel_note_event(host->toplevel, &event);
      }
      return 0;
    case WM_DPICHANGED: {
      /* wParam: LOWORD/HIWORD are the new X/Y-axis DPI (always equal in
       * practice -- Windows has never shipped non-square DPI). lParam:
       * a real RECT* Windows itself suggests for this window at the new
       * DPI, sized/positioned so the window occupies the same *logical*
       * screen area it did before -- both real, documented WM_DPICHANGED
       * facts (winuser.h's own doc comment), confirmed against this
       * machine's own real SDK, not assumed. Applying that suggested
       * rect via SetWindowPos() is required, not optional UX polish: a
       * Per-Monitor-V2-aware app (crtgfx_register_window_class()'s own
       * SetProcessDpiAwarenessContext() call above) that ignores this
       * message leaves its own window the wrong physical size after a
       * real DPI change (e.g. dragging it to a different-DPI monitor) --
       * every real Per-Monitor-V2 app handles this the same way. */
      RECT* suggested = (RECT*)lparam;
      UINT new_dpi = LOWORD(wparam);

      if (host != 0) {
        crtgfx_event event = {0};
        event.type = CRTGFX_EVENT_DPI_SCALE_CHANGED;
        event.data.dpi_scale.scale = (double)new_dpi / 96.0;
        crtgfx_weston_toplevel_note_event(host->toplevel, &event);
      }
      if (suggested != 0) {
        SetWindowPos(hwnd, 0, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
      }
      return 0;
    }
    default:
      return DefWindowProcA(hwnd, message, wparam, lparam);
  }
}

static int crtgfx_register_window_class(void) {
  WNDCLASSEXA cls;

  if (crtgfx_window_class_atom != 0) {
    return CRTGFX_OK;
  }

  /* Process-wide, must run before any window is created (this function
   * always does, via crtgfx_host_window_create()'s own call, before its
   * own CreateWindowExA() a few lines later) -- see this file's own
   * SetProcessDpiAwarenessContext() dllimport comment for why. Return
   * value intentionally ignored: this call can only fail if a DPI
   * awareness mode was already set for the process (e.g. an app
   * manifest declaring one), in which case that earlier declaration
   * already governs and there is nothing wrong to report -- matching
   * real Microsoft-documented guidance for this exact function. */
  (void)SetProcessDpiAwarenessContext(CRTGFX_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  cls = (WNDCLASSEXA){0};
  cls.cbSize = sizeof(cls);
  cls.lpfnWndProc = crtgfx_window_proc;
  cls.hInstance = GetModuleHandleA(0);
  cls.hCursor = LoadCursorA(0, IDC_ARROW);
  cls.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  cls.lpszClassName = crtgfx_window_class_name;

  crtgfx_window_class_atom = RegisterClassExA(&cls);
  if (crtgfx_window_class_atom == 0) {
    return CRTGFX_ERROR_HOST;
  }
  return CRTGFX_OK;
}

/* Releases every DXGI/D3D11 object crtgfx_win_create_swap_chain() may have
 * created, in reverse-acquisition order -- shared between that function's
 * own cleanup-on-failure path and crtgfx_host_window_destroy(), both of
 * which need to tear down exactly the same set of possibly-partial state.
 * Every Release() call is guarded (0 is always safe to skip), matching
 * this file's own established style for every other optional-resource
 * cleanup. */
static void crtgfx_win_destroy_swap_chain(struct crtgfx_host_window* host) {
  if (host->frame_latency_waitable != 0) {
    CloseHandle(host->frame_latency_waitable);
    host->frame_latency_waitable = 0;
  }
  if (host->swap_chain2 != 0) {
    host->swap_chain2->lpVtbl->Release(host->swap_chain2);
    host->swap_chain2 = 0;
  }
  if (host->swap_chain != 0) {
    host->swap_chain->lpVtbl->Release(host->swap_chain);
    host->swap_chain = 0;
  }
  if (host->d3d_context != 0) {
    host->d3d_context->lpVtbl->Release(host->d3d_context);
    host->d3d_context = 0;
  }
  if (host->d3d_device != 0) {
    host->d3d_device->lpVtbl->Release(host->d3d_device);
    host->d3d_device = 0;
  }
}

/* Real D3D11/DXGI bring-up for one window's own presentation surface --
 * see this file's own top comment (crtgfx_dxgi_unknown and friends) for
 * why this whole mechanism exists at all. One device+swap chain per
 * window (not shared process-wide) -- D3D11 device creation is cheap
 * enough that this project has no reason yet to share one across windows
 * the way Linux's single wl_display connection is (window_wayland.c's own
 * comment on that decision).
 *
 * D3D_DRIVER_TYPE_HARDWARE is tried first, falling back to
 * D3D_DRIVER_TYPE_WARP (Windows' own real, always-available software D3D11
 * rasterizer, bundled with the OS since Windows 8 -- needs no GPU driver
 * at all) if that fails -- a real, standard app-initialization pattern,
 * not a guess: a headless/virtualized CI runner with no real GPU driver
 * exposed can and does fail HARDWARE device creation outright, matching
 * this project's own established discipline of not assuming a real GPU is
 * always present (see crtgfx_skia_raster_smoke's own CRTGFX_ERROR_
 * UNSUPPORTED handling for "no display" on Linux). Confirmed for real on
 * this machine (dxgi_probe.c): HARDWARE succeeded here, but the WARP
 * fallback path is exercised as real, reachable code, not dead code, on
 * any host where it doesn't. */
static int crtgfx_win_create_swap_chain(
    struct crtgfx_host_window* host, uint32_t width, uint32_t height) {
  static const int feature_levels[1] = {CRTGFX_D3D_FEATURE_LEVEL_11_0};
  crtgfx_dxgi_factory2* factory;
  crtgfx_dxgi_swap_chain_desc1 desc;
  int feature_level_out;
  HRESULT hr;

  hr = D3D11CreateDevice(0, CRTGFX_D3D_DRIVER_TYPE_HARDWARE, 0, 0, feature_levels, 1, 7,
                         &host->d3d_device, &feature_level_out, &host->d3d_context);
  if (FAILED(hr)) {
    hr = D3D11CreateDevice(0, CRTGFX_D3D_DRIVER_TYPE_WARP, 0, 0, feature_levels, 1, 7,
                           &host->d3d_device, &feature_level_out, &host->d3d_context);
  }
  if (FAILED(hr)) {
    return CRTGFX_ERROR_HOST;
  }

  factory = 0;
  hr = CreateDXGIFactory1(&crtgfx_iid_idxgi_factory2, (void**)&factory);
  if (FAILED(hr) || factory == 0) {
    crtgfx_win_destroy_swap_chain(host);
    return CRTGFX_ERROR_HOST;
  }

  desc = (crtgfx_dxgi_swap_chain_desc1){0};
  desc.Width = width;
  desc.Height = height;
  desc.Format = CRTGFX_DXGI_FORMAT_B8G8R8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.BufferUsage = CRTGFX_DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.BufferCount = 2;
  desc.Scaling = CRTGFX_DXGI_SCALING_STRETCH;
  desc.SwapEffect = CRTGFX_DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  desc.AlphaMode = CRTGFX_DXGI_ALPHA_MODE_IGNORE;
  desc.Flags = CRTGFX_DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

  hr = factory->lpVtbl->CreateSwapChainForHwnd(
      factory, host->d3d_device, host->hwnd, &desc, 0, 0, (void**)&host->swap_chain);
  factory->lpVtbl->Release(factory);
  if (FAILED(hr) || host->swap_chain == 0) {
    crtgfx_win_destroy_swap_chain(host);
    return CRTGFX_ERROR_HOST;
  }
  host->swap_chain_width = width;
  host->swap_chain_height = height;

  hr = host->swap_chain->lpVtbl->QueryInterface(
      host->swap_chain, &crtgfx_iid_idxgi_swapchain2, (void**)&host->swap_chain2);
  if (FAILED(hr) || host->swap_chain2 == 0) {
    crtgfx_win_destroy_swap_chain(host);
    return CRTGFX_ERROR_HOST;
  }
  /* Max latency 1: at most one frame in flight, matching struct crtgfx_
   * host_window::frame_complete_pending's own "at most one outstanding"
   * contract -- present_software() never issues a second Present() while
   * one is already pending. */
  host->swap_chain2->lpVtbl->SetMaximumFrameLatency(host->swap_chain2, 1);
  host->frame_latency_waitable = host->swap_chain2->lpVtbl->GetFrameLatencyWaitableObject(
      host->swap_chain2);
  if (host->frame_latency_waitable == 0) {
    crtgfx_win_destroy_swap_chain(host);
    return CRTGFX_ERROR_HOST;
  }
  return CRTGFX_OK;
}

int crtgfx_host_window_create(const crtgfx_window_desc* desc, crtgfx_weston_toplevel* toplevel) {
  DWORD style;
  RECT rect;
  HWND hwnd;
  crtgfx_host_window* host;

  if (desc == 0 || toplevel == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  if (crtgfx_register_window_class() != CRTGFX_OK) {
    return CRTGFX_ERROR_HOST;
  }

  host = (crtgfx_host_window*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*host));
  if (host == 0) {
    return CRTGFX_ERROR_HOST;
  }
  host->toplevel = toplevel;

  style = WS_OVERLAPPEDWINDOW;
  rect.left = 0;
  rect.top = 0;
  rect.right = (LONG)desc->width;
  rect.bottom = (LONG)desc->height;
  AdjustWindowRectEx(&rect, style, FALSE, 0);

  hwnd = CreateWindowExA(0, crtgfx_window_class_name, desc->title, style, CW_USEDEFAULT,
                         CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, 0, 0,
                         GetModuleHandleA(0), host);
  if (hwnd == 0) {
    HeapFree(GetProcessHeap(), 0, host);
    return CRTGFX_ERROR_HOST;
  }

  host->hwnd = hwnd;
  toplevel->host = host;

  /* Real DXGI swap chain, needed for a genuinely asynchronous
   * CRTGFX_EVENT_FRAME_COMPLETE -- see crtgfx_win_create_swap_chain()'s
   * own comment. Sized against the window's own real client rect (not
   * desc->width/height, which is the *outer* size AdjustWindowRectEx()
   * expanded above to account for the title bar/borders) -- matches
   * every other backend's own "ask the host for the real client size,
   * don't assume the caller's requested size" discipline. */
  {
    RECT client_rect;
    uint32_t client_width = desc->width;
    uint32_t client_height = desc->height;

    if (GetClientRect(hwnd, &client_rect)) {
      client_width = (uint32_t)(client_rect.right - client_rect.left);
      client_height = (uint32_t)(client_rect.bottom - client_rect.top);
    }
    if (client_width == 0) {
      client_width = 1;
    }
    if (client_height == 0) {
      client_height = 1;
    }
    if (crtgfx_win_create_swap_chain(host, client_width, client_height) != CRTGFX_OK) {
      DestroyWindow(hwnd);
      HeapFree(GetProcessHeap(), 0, host);
      return CRTGFX_ERROR_HOST;
    }
  }

  host->next = crtgfx_win_windows;
  crtgfx_win_windows = host;

  if ((desc->flags & CRTGFX_WINDOW_VISIBLE) != 0) {
    return crtgfx_host_window_show(host);
  }
  return CRTGFX_OK;
}

void crtgfx_host_window_destroy(crtgfx_host_window* host) {
  struct crtgfx_host_window** link = &crtgfx_win_windows;

  while (*link != 0) {
    if (*link == host) {
      *link = host->next;
      break;
    }
    link = &(*link)->next;
  }
  crtgfx_win_destroy_swap_chain(host);
  if (host->hwnd != 0) {
    DestroyWindow(host->hwnd);
  }
  HeapFree(GetProcessHeap(), 0, host);
}

int crtgfx_host_window_show(crtgfx_host_window* host) {
  if (host == 0 || host->hwnd == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  ShowWindow(host->hwnd, SW_SHOWNORMAL);
  UpdateWindow(host->hwnd);
  return CRTGFX_OK;
}

int crtgfx_host_window_dispatch(uint32_t timeout_ms) {
  DWORD start;
  MSG message;
  struct crtgfx_host_window* w;

  start = GetTickCount();
  do {
    while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
      TranslateMessage(&message);
      DispatchMessageA(&message);
    }
    /* Real, asynchronous CRTGFX_EVENT_FRAME_COMPLETE delivery -- every
     * live window is checked once per dispatch() cycle, the normal path
     * this event is meant to be observed through (matching Linux's own
     * wl_callback::done arriving on a later crtgfx_host_window_dispatch()
     * call, not synchronously with the present that requested it). See
     * crtgfx_win_poll_frame_complete()'s own comment. */
    for (w = crtgfx_win_windows; w != 0; w = w->next) {
      crtgfx_win_poll_frame_complete(w);
    }
    if (timeout_ms == 0) {
      break;
    }
    Sleep(1);
  } while ((GetTickCount() - start) < timeout_ms);

  return CRTGFX_OK;
}

int crtgfx_host_window_get_size(crtgfx_host_window* host, uint32_t* out_width, uint32_t* out_height) {
  RECT rect;

  if (host == 0 || out_width == 0 || out_height == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  if (host->hwnd != 0 && GetClientRect(host->hwnd, &rect)) {
    crtgfx_weston_toplevel_note_size(host->toplevel, (uint32_t)(rect.right - rect.left),
                                     (uint32_t)(rect.bottom - rect.top));
  }
  *out_width = host->toplevel->width;
  *out_height = host->toplevel->height;
  return CRTGFX_OK;
}

int crtgfx_host_window_present_software(
    crtgfx_host_window* host, const void* pixels, uint32_t width, uint32_t height, uint32_t stride,
    const crtgfx_damage_rect* damage_rects, uint32_t damage_rect_count) {
  crtgfx_d3d11_texture2d* back_buffer;
  HRESULT hr;

  if (host == 0 || host->hwnd == 0 || host->swap_chain == 0 || pixels == 0 || width == 0 ||
      height == 0 || stride < width * 4u) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }

  /* Best-effort extra check before issuing a new Present() -- see
   * crtgfx_win_poll_frame_complete()'s own comment for why this is
   * needed in addition to crtgfx_host_window_dispatch()'s own per-cycle
   * poll (present_software() can legitimately be called faster than
   * dispatch() ever runs in between, e.g. a caller that batches several
   * frames before its next pump_events() call). */
  crtgfx_win_poll_frame_complete(host);

  back_buffer = 0;
  hr = host->swap_chain->lpVtbl->GetBuffer(
      host->swap_chain, 0, &crtgfx_iid_id3d11_texture2d, (void**)&back_buffer);
  if (FAILED(hr) || back_buffer == 0) {
    return CRTGFX_ERROR_HOST;
  }

  if (damage_rects == 0 || damage_rect_count == 0) {
    /* pDstBox = 0 (whole subresource): SrcRowPitch is this buffer's own
     * real stride either way -- UpdateSubresource has no separate
     * "source width" parameter, it infers the copied extent from the
     * destination (sub)resource's own real dimensions, which the swap
     * chain's width/height (kept in sync with the window's real client
     * size via crtgfx_win_resize_swap_chain(), called from WM_SIZE) match
     * by construction. */
    host->d3d_context->lpVtbl->UpdateSubresource(
        host->d3d_context, back_buffer, 0, 0, pixels, stride, 0);
  } else {
    /* One real UpdateSubresource per damage rect, each with its own
     * D3D11_BOX -- a genuine partial-present optimization (D3D11 fully
     * supports sub-rect texture updates), matching the same feature the
     * previous StretchDIBits-based implementation had. The one subtlety
     * D3D11's own documentation is easy to miss: pSrcData/SrcRowPitch are
     * NOT relative to the box -- pSrcData must still point at this
     * *rect's own* first texel within the full source buffer (computed
     * below via stride/4-byte-pixel math), but SrcRowPitch stays the
     * *whole buffer's own* real stride, not the rect's own width*4 --
     * confirmed against real D3D11 UpdateSubresource documentation, not
     * guessed, and matches how this same distinction was reasoned through
     * before landing here (see HISTORY.md). */
    uint32_t i;

    for (i = 0; i < damage_rect_count; ++i) {
      crtgfx_d3d11_box box;
      const uint8_t* rect_origin;

      if (damage_rects[i].width == 0u || damage_rects[i].height == 0u) {
        continue; /* a real but degenerate rect -- no-op, not a D3D11 error */
      }
      box.left = damage_rects[i].x;
      box.top = damage_rects[i].y;
      box.front = 0;
      box.right = damage_rects[i].x + damage_rects[i].width;
      box.bottom = damage_rects[i].y + damage_rects[i].height;
      box.back = 1;
      rect_origin = (const uint8_t*)pixels + (size_t)damage_rects[i].y * stride +
                    (size_t)damage_rects[i].x * 4u;
      host->d3d_context->lpVtbl->UpdateSubresource(
          host->d3d_context, back_buffer, 0, &box, rect_origin, stride, 0);
    }
  }
  back_buffer->lpVtbl->Release(back_buffer);

  /* SyncInterval=1: present synchronized to the next vblank, matching
   * this backend's own genuine intent (a real, compositor-paced present,
   * not "as fast as possible") now that it goes through a real swap chain
   * at all. DXGI_STATUS_OCCLUDED and other non-error DXGI status codes
   * (SUCCEEDED(hr) true, hr != S_OK) are real, normal, documented outcomes
   * -- e.g. the window is currently occluded/not visibly composited --
   * not failures; only a genuine negative HRESULT is treated as one here,
   * matching real DXGI application conventions. */
  hr = host->swap_chain->lpVtbl->Present(host->swap_chain, 1, 0);
  if (FAILED(hr)) {
    return CRTGFX_ERROR_HOST;
  }

  /* CRTGFX_EVENT_FRAME_COMPLETE is deliberately NOT fired here -- unlike
   * this file's own previous StretchDIBits-based implementation (genuinely
   * synchronous, since GDI's own blit really did finish by the time that
   * call returned), Present() on a real DXGI flip-model swap chain does
   * not mean the previously-presented buffer has been retired yet. This
   * present's own completion is only known once host->frame_latency_
   * waitable actually signals -- crtgfx_host_window_dispatch()'s own
   * per-cycle poll (or this function's own best-effort check above, on
   * the *next* present_software() call) is what actually fires the event,
   * asynchronously, the same shape Linux's wl_surface::frame/wl_callback::
   * done and macOS's CATransaction completion block already use. See
   * crtgfx/window.h's own CRTGFX_EVENT_FRAME_COMPLETE doc comment. */
  host->frame_complete_pending = 1;
  return CRTGFX_OK;
}
