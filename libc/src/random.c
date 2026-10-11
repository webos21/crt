#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/random.h>
#include <unistd.h>

ssize_t getrandom(void* buffer, size_t buffer_size, unsigned int flags) {
  unsigned char* out = (unsigned char*)buffer;
  size_t done = 0;
  int fd;

  if (buffer == 0 && buffer_size != 0) {
    return __set_errno(EFAULT);
  }
  if ((flags & ~(GRND_NONBLOCK | GRND_RANDOM | GRND_INSECURE)) != 0) {
    return __set_errno(EINVAL);
  }

  fd = open((flags & GRND_RANDOM) ? "/dev/random" : "/dev/urandom", O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    return -1;
  }

  while (done < buffer_size) {
    ssize_t n = read(fd, out + done, buffer_size - done);
    if (n < 0) {
      int saved_errno = errno;
      close(fd);
      errno = saved_errno;
      return done == 0 ? -1 : (ssize_t)done;
    }
    if (n == 0) break;
    done += (size_t)n;
  }
  close(fd);
  return (ssize_t)done;
}

int getentropy(void* buffer, size_t buffer_size) {
  size_t done = 0;

  if (buffer_size > 256) {
    return __set_errno(EIO);
  }
  while (done < buffer_size) {
    ssize_t n = getrandom((unsigned char*)buffer + done, buffer_size - done, 0);
    if (n < 0) return -1;
    if (n == 0) return __set_errno(EIO);
    done += (size_t)n;
  }
  return 0;
}

// rand()/srand() (ISO C) and random()/srandom() (POSIX): a 48-bit linear
// congruential generator using the well-known POSIX drand48/rand48
// family constants (multiplier 0x5DEECE66D, increment 0xB -- the same
// ones behind drand48()/jrand48() and, not coincidentally, java.util.
// Random), not a literal port of BSD's own proprietary additive-
// feedback random() implementation. POSIX does not mandate a specific
// output sequence for either API -- only that it be a reproducible
// pseudo-random sequence given the same seed within a run -- which this
// provides. Needed for onetrueawk's rand()/srand() builtins (see
// shell/awk/); rand.cpp in Bionic itself defines rand()/srand() as
// thin wrappers over random()/srandom() for the same reason ("the BSD
// rand/srand is very weak"), a pattern kept here too.
static unsigned long long random_state = 1;

void srandom(unsigned int seed) {
  random_state = (unsigned long long)seed;
}

long random(void) {
  random_state = (random_state * 0x5DEECE66DULL + 0xBULL) & 0xFFFFFFFFFFFFULL;
  return (long)((random_state >> 17) & 0x7fffffffULL);
}

// initstate()/setstate(): the caller-supplied buffer holds the generator
// state of one independent sequence (the 48-bit state, saved when another
// sequence is selected). BSD's buffers also select a different feedback
// polynomial by size; this generator has only one, so any size >= 8 bytes
// is accepted and the size has no other effect. The first call returns the
// internal buffer that holds the initial sequence, as BSD does.
static unsigned char random_internal_state[8];
static char* random_current = (char*)random_internal_state;

static void random_save(char* buffer) {
  unsigned long long state = random_state;
  for (int i = 0; i < 8; i++) {
    buffer[i] = (char)(state >> (8 * i));
  }
}

static void random_load(const char* buffer) {
  unsigned long long state = 0;
  for (int i = 0; i < 8; i++) {
    state |= (unsigned long long)(unsigned char)buffer[i] << (8 * i);
  }
  random_state = state;
}

char* initstate(unsigned int seed, char* state, size_t size) {
  char* previous = random_current;
  if (state == NULL || size < 8) {
    return NULL;
  }
  random_save(previous);
  random_current = state;
  srandom(seed);
  random_save(state);
  return previous;
}

char* setstate(char* state) {
  char* previous = random_current;
  if (state == NULL) {
    return NULL;
  }
  random_save(previous);
  random_load(state);
  random_current = state;
  return previous;
}

void srand(unsigned int seed) {
  srandom(seed);
}

int rand(void) {
  return (int)random();
}
