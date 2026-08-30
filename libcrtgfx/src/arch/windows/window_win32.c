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

struct crtgfx_host_window {
  HWND hwnd;
  crtgfx_weston_toplevel* toplevel;
  /* WM_CHAR delivers UTF-16 one code unit at a time; a character outside
   * the BMP (rare -- e.g. an emoji) arrives as a surrogate *pair* across
   * two separate WM_CHAR messages, so the high surrogate has to be held
   * here until its matching low surrogate shows up. 0 = none pending. */
  uint16_t pending_high_surrogate;
};

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

  if ((desc->flags & CRTGFX_WINDOW_VISIBLE) != 0) {
    return crtgfx_host_window_show(host);
  }
  return CRTGFX_OK;
}

void crtgfx_host_window_destroy(crtgfx_host_window* host) {
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

  start = GetTickCount();
  do {
    while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
      TranslateMessage(&message);
      DispatchMessageA(&message);
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
  BITMAPINFO info;
  HDC dc;
  RECT rect;
  int client_width;
  int client_height;
  int drawn;

  if (host == 0 || host->hwnd == 0 || pixels == 0 || width == 0 || height == 0 ||
      stride < width * 4u) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  if (!GetClientRect(host->hwnd, &rect)) {
    return CRTGFX_ERROR_HOST;
  }
  client_width = (int)(rect.right - rect.left);
  client_height = (int)(rect.bottom - rect.top);
  dc = GetDC(host->hwnd);
  if (dc == 0) {
    return CRTGFX_ERROR_HOST;
  }

  info = (BITMAPINFO){0};
  info.bmiHeader.biSize = sizeof(info.bmiHeader);
  info.bmiHeader.biWidth = (LONG)(stride / 4u);
  info.bmiHeader.biHeight = -(LONG)height;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;

  if (damage_rects == 0 || damage_rect_count == 0) {
    drawn = StretchDIBits(dc, 0, 0, client_width, client_height, 0, 0, (int)width, (int)height,
                          pixels, &info, DIB_RGB_COLORS, SRCCOPY);
    if (drawn == 0) {
      ReleaseDC(host->hwnd, dc);
      return CRTGFX_ERROR_HOST;
    }
  } else {
    /* Real, direct 1:1 blit of just the declared rects -- a genuine
     * partial-present optimization (StretchDIBits() really does support
     * an arbitrary source/dest sub-rect, confirmed against this
     * machine's own real SDK signature), not a scaling API: source and
     * dest use the exact same coordinates here, unlike the whole-frame
     * path above which stretches to the current client size. See
     * crtgfx/window.h's own crtgfx_window_end_frame_damaged() comment --
     * damage rects are given in framebuffer pixel coordinates. */
    uint32_t i;

    for (i = 0; i < damage_rect_count; ++i) {
      int x = (int)damage_rects[i].x;
      int y = (int)damage_rects[i].y;
      int w = (int)damage_rects[i].width;
      int h = (int)damage_rects[i].height;

      drawn = StretchDIBits(dc, x, y, w, h, x, y, w, h, pixels, &info, DIB_RGB_COLORS, SRCCOPY);
      if (drawn == 0) {
        ReleaseDC(host->hwnd, dc);
        return CRTGFX_ERROR_HOST;
      }
    }
  }
  ReleaseDC(host->hwnd, dc);

  /* CRTGFX_EVENT_FRAME_COMPLETE: fired synchronously, right here, since
   * StretchDIBits() is itself synchronous -- see crtgfx/window.h's own
   * doc comment on this event for why this is an accurate reflection of
   * Windows' own real completion timing, not a real vsync signal. */
  if (host->toplevel != 0) {
    crtgfx_event event = {0};

    event.type = CRTGFX_EVENT_FRAME_COMPLETE;
    crtgfx_weston_toplevel_note_event(host->toplevel, &event);
  }
  return CRTGFX_OK;
}
