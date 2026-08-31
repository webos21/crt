#include "crtmedia/audio.h"

#include <string.h>

void crtmedia_audio_buffer_release(crtmedia_audio_buffer* buffer) {
  if (buffer == NULL) {
    return;
  }
  if (buffer->release != NULL) {
    buffer->release(buffer, buffer->release_context);
  }
  memset(buffer, 0, sizeof(*buffer));
}
