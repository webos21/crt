/* The crt-egl port: the Khronos headers compile and the CRT "no GPU" libEGL/libGLESv2 answer
 * every call with failure instead of a context. */
#include <stdio.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

int main(void) {
  EGLint major = 0;
  EGLint minor = 0;
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

  if (display != EGL_NO_DISPLAY || eglInitialize(display, &major, &minor) != EGL_FALSE ||
      eglGetError() != EGL_NOT_INITIALIZED || sizeof(EGLint) != 4) {
    fprintf(stderr, "egl_headers_test: the stub did not report 'no GPU'\n");
    return 1;
  }
  printf("egl_headers_test: ok\n");
  return 0;
}
