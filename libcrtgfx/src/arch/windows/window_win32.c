#include "wayland_weston_internal.h"

#include <stddef.h>
#include <stdint.h>

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
#define WM_CLOSE 0x0010u
#define WM_PAINT 0x000fu
#define WM_NCCREATE 0x0081u
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

#define LOWORD(value) ((WORD)((uintptr_t)(value) & 0xffffu))
#define HIWORD(value) ((WORD)(((uintptr_t)(value) >> 16) & 0xffffu))

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
};

static const char crtgfx_window_class_name[] = "crtgfx_window";
static ATOM crtgfx_window_class_atom;

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
    case WM_PAINT: {
      PAINTSTRUCT paint;
      BeginPaint(hwnd, &paint);
      EndPaint(hwnd, &paint);
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
    crtgfx_host_window* host, const void* pixels, uint32_t width, uint32_t height, uint32_t stride) {
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

  drawn = StretchDIBits(dc, 0, 0, client_width, client_height, 0, 0, (int)width, (int)height,
                        pixels, &info, DIB_RGB_COLORS, SRCCOPY);
  ReleaseDC(host->hwnd, dc);
  if (drawn == 0) {
    return CRTGFX_ERROR_HOST;
  }
  return CRTGFX_OK;
}
