#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>

/* Real macOS/XNU termios support, via the same TIOCGETA/TIOCSETA{,W,F}/
 * TIOCDRAIN/TIOCFLUSH/TIOCSTART/TIOCSTOP/TIOCIXON/TIOCIXOFF/TIOCSBRK/
 * TIOCCBRK ioctls fd.c's isatty() already proved out for real (TIOCGETA as
 * a pure "is this fd a tty" probe -- see CRT_DARWIN_TIOCGETA there).
 * libc/src/termios.c's portable tcgetattr()/tcsetattr()/tcdrain()/
 * tcflow()/tcflush()/tcsendbreak() used to fall through to pure
 * hardcoded-value/no-op stubs on macOS: tcgetattr() always returned one
 * fixed struct regardless of fd state, and tcsetattr() silently discarded
 * everything it was asked to set. Confirmed as a real, general round-trip
 * bug on real macOS hardware (tests/termios_echo_roundtrip_test.c fails
 * with CRT_USE_HOST_TTY=1 against a real pty, before this file existed) --
 * the same class of bug Linux's own equivalent
 * (libc/src/arch/linux/common/termios.c) already fixed.
 *
 * Every ioctl request number, struct layout, and flag-bit value below was
 * cross-checked directly against this project's real macOS build host: the
 * Xcode SDK's own <sys/termios.h>/<sys/ttycom.h> headers, plus a tiny
 * host-native (real system clang, real libSystem) reference program
 * compiled and run on this machine printing every constant used here --
 * not copied from memory, not guessed. Real Darwin's own struct termios
 * differs from this project's public, Bionic-shaped <termios.h> in every
 * way that matters:
 *  - tcflag_t/speed_t are 8-byte `unsigned long` here, not this project's
 *    4-byte `unsigned int`, and there is no c_line field
 *    (sizeof(struct termios) == 72 confirmed both ways, see the static
 *    assert below);
 *  - NCCS is 20, not 32, and every V* control-character index (VINTR,
 *    VEOF, VMIN, ...) sits at a different offset than this project's
 *    Bionic/Linux-shaped indices -- see kBionicCcToDarwin/kDarwinCcToBionic;
 *  - every c_iflag/c_oflag/c_cflag/c_lflag bit this project defines sits
 *    at a different bit position than the same-named Bionic bit -- see
 *    the per-category translation functions below;
 *  - c_ispeed/c_ospeed are real literal baud numbers (9600, 38400, ...),
 *    not this project's Bionic/Linux-style small-integer B-codes (e.g.
 *    this project's own B9600 is the literal integer 13, not 9600) -- see
 *    bionic_speed_to_darwin()/darwin_speed_to_bionic().
 * A handful of Bionic-only bits (IUCLC, OLCUC, XCASE -- all SysV-only
 * legacy concepts) and Darwin-only bits (CIGNORE, ALTWERASE, NOKERNINFO,
 * the hardware flow-control bits CCTS_OFLOW/CRTS_IFLOW/CDTR_IFLOW/
 * CDSR_OFLOW/CCAR_OFLOW, VDSUSP/VSTATUS) have no counterpart on the other
 * side and are silently dropped in translation; each is called out at its
 * own drop site below rather than guessed at.
 */

#define CRT_DARWIN_NCCS 20

struct crt_darwin_termios {
  unsigned long c_iflag;
  unsigned long c_oflag;
  unsigned long c_cflag;
  unsigned long c_lflag;
  unsigned char c_cc[CRT_DARWIN_NCCS];
  unsigned long c_ispeed;
  unsigned long c_ospeed;
};

typedef char crt_darwin_termios_size_check[
    sizeof(struct crt_darwin_termios) == 72 ? 1 : -1];

/* _IOC('t', N, sizeof(struct termios)==72)/_IO('t', N) request numbers,
 * matching fd.c's own established style of hardcoding the computed
 * literal rather than reimplementing <sys/ioccom.h>'s macros in this
 * tree (see CRT_DARWIN_TIOCGETA there, which this file's own value below
 * matches exactly -- cross-verified, not just copy-pasted). */
#define CRT_DARWIN_TIOCGETA 0x40487413UL  /* _IOR('t', 19, struct termios) */
#define CRT_DARWIN_TIOCSETA 0x80487414UL  /* _IOW('t', 20, struct termios) */
#define CRT_DARWIN_TIOCSETAW 0x80487415UL /* _IOW('t', 21, struct termios) */
#define CRT_DARWIN_TIOCSETAF 0x80487416UL /* _IOW('t', 22, struct termios) */
#define CRT_DARWIN_TIOCFLUSH 0x80047410UL /* _IOW('t', 16, int) */
#define CRT_DARWIN_TIOCDRAIN 0x2000745eUL /* _IO('t', 94) */
#define CRT_DARWIN_TIOCSTART 0x2000746eUL /* _IO('t', 110) */
#define CRT_DARWIN_TIOCSTOP 0x2000746fUL  /* _IO('t', 111) */
#define CRT_DARWIN_TIOCIXON 0x20007481UL  /* _IO('t', 129) */
#define CRT_DARWIN_TIOCIXOFF 0x20007480UL /* _IO('t', 128) */
#define CRT_DARWIN_TIOCSBRK 0x2000747bUL  /* _IO('t', 123) */
#define CRT_DARWIN_TIOCCBRK 0x2000747aUL  /* _IO('t', 122) */

/* tcflush()'s TCIFLUSH/TCOFLUSH/TCIOFLUSH and tcflow()'s TCOOFF/TCOON/
 * TCIOFF/TCION are all off-by-one between this project's Bionic/glibc-
 * numbered <termios.h> (TCIFLUSH=0/TCOFLUSH=1/TCIOFLUSH=2, TCOOFF=0/
 * TCOON=1/TCIOFF=2/TCION=3) and real Darwin's own (TCIFLUSH=1/TCOFLUSH=2/
 * TCIOFLUSH=3) -- confirmed directly against the host reference program,
 * not assumed from the numbers merely "looking similar". tcflow()'s
 * Darwin action values themselves are never actually passed to an ioctl
 * (see __crt_sys_tcflow() below, which dispatches to four different
 * ioctls instead), so only the TCIFLUSH/TCOFLUSH/TCIOFLUSH set needs a
 * translated constant here. */
#define CRT_DARWIN_TCIFLUSH 1
#define CRT_DARWIN_TCOFLUSH 2
#define CRT_DARWIN_TCIOFLUSH 3

#define CRT_DARWIN_IGNBRK 0x00000001UL
#define CRT_DARWIN_BRKINT 0x00000002UL
#define CRT_DARWIN_IGNPAR 0x00000004UL
#define CRT_DARWIN_PARMRK 0x00000008UL
#define CRT_DARWIN_INPCK 0x00000010UL
#define CRT_DARWIN_ISTRIP 0x00000020UL
#define CRT_DARWIN_INLCR 0x00000040UL
#define CRT_DARWIN_IGNCR 0x00000080UL
#define CRT_DARWIN_ICRNL 0x00000100UL
#define CRT_DARWIN_IXON 0x00000200UL
#define CRT_DARWIN_IXOFF 0x00000400UL
#define CRT_DARWIN_IXANY 0x00000800UL
#define CRT_DARWIN_IMAXBEL 0x00002000UL
#define CRT_DARWIN_IUTF8 0x00004000UL

#define CRT_DARWIN_OPOST 0x00000001UL
#define CRT_DARWIN_ONLCR 0x00000002UL
#define CRT_DARWIN_OCRNL 0x00000010UL
#define CRT_DARWIN_ONOCR 0x00000020UL
#define CRT_DARWIN_ONLRET 0x00000040UL
#define CRT_DARWIN_OFILL 0x00000080UL
#define CRT_DARWIN_OFDEL 0x00020000UL

#define CRT_DARWIN_CSIZE 0x00000300UL
#define CRT_DARWIN_CSTOPB 0x00000400UL
#define CRT_DARWIN_CREAD 0x00000800UL
#define CRT_DARWIN_PARENB 0x00001000UL
#define CRT_DARWIN_PARODD 0x00002000UL
#define CRT_DARWIN_HUPCL 0x00004000UL
#define CRT_DARWIN_CLOCAL 0x00008000UL

#define CRT_DARWIN_ECHOKE 0x00000001UL
#define CRT_DARWIN_ECHOE 0x00000002UL
#define CRT_DARWIN_ECHOK 0x00000004UL
#define CRT_DARWIN_ECHO 0x00000008UL
#define CRT_DARWIN_ECHONL 0x00000010UL
#define CRT_DARWIN_ECHOPRT 0x00000020UL
#define CRT_DARWIN_ECHOCTL 0x00000040UL
#define CRT_DARWIN_ISIG 0x00000080UL
#define CRT_DARWIN_ICANON 0x00000100UL
#define CRT_DARWIN_IEXTEN 0x00000400UL
#define CRT_DARWIN_EXTPROC 0x00000800UL
#define CRT_DARWIN_TOSTOP 0x00400000UL
#define CRT_DARWIN_FLUSHO 0x00800000UL
#define CRT_DARWIN_PENDIN 0x20000000UL
#define CRT_DARWIN_NOFLSH 0x80000000UL

/* Bionic V* index (0..16, this project's own include/termios.h) -> Darwin
 * V* index (0..19, real <sys/termios.h>), or -1 where Darwin has no
 * equivalent slot. VSWTC (Bionic index 7, a SysV/Linux-only "switch"
 * character concept) is the only Bionic-named control character Darwin
 * has nothing for. */
static const signed char kBionicCcToDarwin[17] = {
    8,  /* VINTR    -> Darwin VINTR (8) */
    9,  /* VQUIT    -> Darwin VQUIT (9) */
    3,  /* VERASE   -> Darwin VERASE (3) */
    5,  /* VKILL    -> Darwin VKILL (5) */
    0,  /* VEOF     -> Darwin VEOF (0) */
    17, /* VTIME    -> Darwin VTIME (17) */
    16, /* VMIN     -> Darwin VMIN (16) */
    -1, /* VSWTC    -> no Darwin equivalent */
    12, /* VSTART   -> Darwin VSTART (12) */
    13, /* VSTOP    -> Darwin VSTOP (13) */
    10, /* VSUSP    -> Darwin VSUSP (10) */
    1,  /* VEOL     -> Darwin VEOL (1) */
    6,  /* VREPRINT -> Darwin VREPRINT (6) */
    15, /* VDISCARD -> Darwin VDISCARD (15) */
    4,  /* VWERASE  -> Darwin VWERASE (4) */
    14, /* VLNEXT   -> Darwin VLNEXT (14) */
    2,  /* VEOL2    -> Darwin VEOL2 (2) */
};

/* Reverse of the table above. Darwin's VDSUSP (11) and VSTATUS (18) have
 * no Bionic-named counterpart in this project's <termios.h> (indices 7
 * and 19 are Darwin's own documented "spare" slots), so they map to -1
 * and are dropped going Darwin->Bionic. */
static const signed char kDarwinCcToBionic[CRT_DARWIN_NCCS] = {
    4,  /* VEOF(0)     -> Bionic VEOF (4) */
    11, /* VEOL(1)     -> Bionic VEOL (11) */
    16, /* VEOL2(2)    -> Bionic VEOL2 (16) */
    2,  /* VERASE(3)   -> Bionic VERASE (2) */
    14, /* VWERASE(4)  -> Bionic VWERASE (14) */
    3,  /* VKILL(5)    -> Bionic VKILL (3) */
    12, /* VREPRINT(6) -> Bionic VREPRINT (12) */
    -1, /* spare(7) */
    0,  /* VINTR(8)    -> Bionic VINTR (0) */
    1,  /* VQUIT(9)    -> Bionic VQUIT (1) */
    10, /* VSUSP(10)   -> Bionic VSUSP (10) */
    -1, /* VDSUSP(11)  -> no Bionic equivalent */
    8,  /* VSTART(12)  -> Bionic VSTART (8) */
    9,  /* VSTOP(13)   -> Bionic VSTOP (9) */
    15, /* VLNEXT(14)  -> Bionic VLNEXT (15) */
    13, /* VDISCARD(15)-> Bionic VDISCARD (13) */
    6,  /* VMIN(16)    -> Bionic VMIN (6) */
    5,  /* VTIME(17)   -> Bionic VTIME (5) */
    -1, /* VSTATUS(18) -> no Bionic equivalent */
    -1, /* spare(19) */
};

/* This project's public <termios.h> only defines B0/B9600/B38400/B115200
 * (this port's only tested/supported speeds); extend this table, matching
 * that header's own B* set, if a real consumer needs another one. Any
 * value not in the table passes straight through, which is only correct
 * if the caller is already using a literal baud number rather than one of
 * Bionic/Linux's other small-integer B-codes this project doesn't
 * currently expose a macro for. */
static unsigned long bionic_speed_to_darwin(speed_t speed) {
  switch (speed) {
    case B0:
      return 0;
    case B9600:
      return 9600;
    case B38400:
      return 38400;
    case B115200:
      return 115200;
    default:
      return (unsigned long)speed;
  }
}

static speed_t darwin_speed_to_bionic(unsigned long speed) {
  switch (speed) {
    case 0:
      return B0;
    case 9600:
      return B9600;
    case 38400:
      return B38400;
    case 115200:
      return B115200;
    default:
      return (speed_t)speed;
  }
}

static unsigned long bionic_iflag_to_darwin(tcflag_t f) {
  unsigned long d = 0;

  if (f & IGNBRK) d |= CRT_DARWIN_IGNBRK;
  if (f & BRKINT) d |= CRT_DARWIN_BRKINT;
  if (f & IGNPAR) d |= CRT_DARWIN_IGNPAR;
  if (f & PARMRK) d |= CRT_DARWIN_PARMRK;
  if (f & INPCK) d |= CRT_DARWIN_INPCK;
  if (f & ISTRIP) d |= CRT_DARWIN_ISTRIP;
  if (f & INLCR) d |= CRT_DARWIN_INLCR;
  if (f & IGNCR) d |= CRT_DARWIN_IGNCR;
  if (f & ICRNL) d |= CRT_DARWIN_ICRNL;
  if (f & IXON) d |= CRT_DARWIN_IXON;
  if (f & IXOFF) d |= CRT_DARWIN_IXOFF;
  if (f & IXANY) d |= CRT_DARWIN_IXANY;
  if (f & IMAXBEL) d |= CRT_DARWIN_IMAXBEL;
  if (f & IUTF8) d |= CRT_DARWIN_IUTF8;
  /* IUCLC (SysV-only legacy bit) has no Darwin equivalent -- dropped. */
  return d;
}

static tcflag_t darwin_iflag_to_bionic(unsigned long d) {
  tcflag_t f = 0;

  if (d & CRT_DARWIN_IGNBRK) f |= IGNBRK;
  if (d & CRT_DARWIN_BRKINT) f |= BRKINT;
  if (d & CRT_DARWIN_IGNPAR) f |= IGNPAR;
  if (d & CRT_DARWIN_PARMRK) f |= PARMRK;
  if (d & CRT_DARWIN_INPCK) f |= INPCK;
  if (d & CRT_DARWIN_ISTRIP) f |= ISTRIP;
  if (d & CRT_DARWIN_INLCR) f |= INLCR;
  if (d & CRT_DARWIN_IGNCR) f |= IGNCR;
  if (d & CRT_DARWIN_ICRNL) f |= ICRNL;
  if (d & CRT_DARWIN_IXON) f |= IXON;
  if (d & CRT_DARWIN_IXOFF) f |= IXOFF;
  if (d & CRT_DARWIN_IXANY) f |= IXANY;
  if (d & CRT_DARWIN_IMAXBEL) f |= IMAXBEL;
  if (d & CRT_DARWIN_IUTF8) f |= IUTF8;
  return f;
}

static unsigned long bionic_oflag_to_darwin(tcflag_t f) {
  unsigned long d = 0;

  if (f & OPOST) d |= CRT_DARWIN_OPOST;
  if (f & ONLCR) d |= CRT_DARWIN_ONLCR;
  if (f & OCRNL) d |= CRT_DARWIN_OCRNL;
  if (f & ONOCR) d |= CRT_DARWIN_ONOCR;
  if (f & ONLRET) d |= CRT_DARWIN_ONLRET;
  if (f & OFILL) d |= CRT_DARWIN_OFILL;
  if (f & OFDEL) d |= CRT_DARWIN_OFDEL;
  /* OLCUC (SysV-only) and the NLDLY/CRDLY/TABDLY/BSDLY/VTDLY/FFDLY
   * multi-bit output-delay groups have no well-defined Darwin equivalent
   * worth translating bit-for-bit: Darwin's own <sys/termios.h> marks its
   * versions of these "unimplemented ... will currently result in
   * unexpected behaviour", so they're dropped rather than guessed at. */
  return d;
}

static tcflag_t darwin_oflag_to_bionic(unsigned long d) {
  tcflag_t f = 0;

  if (d & CRT_DARWIN_OPOST) f |= OPOST;
  if (d & CRT_DARWIN_ONLCR) f |= ONLCR;
  if (d & CRT_DARWIN_OCRNL) f |= OCRNL;
  if (d & CRT_DARWIN_ONOCR) f |= ONOCR;
  if (d & CRT_DARWIN_ONLRET) f |= ONLRET;
  if (d & CRT_DARWIN_OFILL) f |= OFILL;
  if (d & CRT_DARWIN_OFDEL) f |= OFDEL;
  return f;
}

static unsigned long bionic_cflag_to_darwin(tcflag_t f) {
  unsigned long d = 0;
  /* CSIZE is a 2-bit field on both sides (CS5=0/CS6=1/CS7=2/CS8=3 as an
   * index), just at different bit offsets -- Bionic's CSIZE mask sits at
   * bit 4, Darwin's at bit 8. */
  unsigned long csize_index = (f & CSIZE) >> 4;

  d |= (csize_index << 8) & CRT_DARWIN_CSIZE;
  if (f & CSTOPB) d |= CRT_DARWIN_CSTOPB;
  if (f & CREAD) d |= CRT_DARWIN_CREAD;
  if (f & PARENB) d |= CRT_DARWIN_PARENB;
  if (f & PARODD) d |= CRT_DARWIN_PARODD;
  if (f & HUPCL) d |= CRT_DARWIN_HUPCL;
  if (f & CLOCAL) d |= CRT_DARWIN_CLOCAL;
  /* CBAUD/CBAUDEX/CIBAUD/CMSPAR/CRTSCTS (Linux packs baud rate and RTS/CTS
   * flow control into c_cflag bits) have no Darwin equivalent -- Darwin
   * keeps baud rate in the separate c_ispeed/c_ospeed fields (already
   * handled by bionic_speed_to_darwin()) and has no c_cflag-level RTS/CTS
   * concept at all (CCTS_OFLOW/CRTS_IFLOW live in c_cflag too, but as
   * genuinely different hardware-flow-control bits, not a CRTSCTS alias
   * -- not translated, no real consumer needs it yet). */
  return d;
}

static tcflag_t darwin_cflag_to_bionic(unsigned long d) {
  tcflag_t f = 0;
  unsigned long csize_index = (d & CRT_DARWIN_CSIZE) >> 8;

  f |= (tcflag_t)((csize_index << 4) & CSIZE);
  if (d & CRT_DARWIN_CSTOPB) f |= CSTOPB;
  if (d & CRT_DARWIN_CREAD) f |= CREAD;
  if (d & CRT_DARWIN_PARENB) f |= PARENB;
  if (d & CRT_DARWIN_PARODD) f |= PARODD;
  if (d & CRT_DARWIN_HUPCL) f |= HUPCL;
  if (d & CRT_DARWIN_CLOCAL) f |= CLOCAL;
  return f;
}

static unsigned long bionic_lflag_to_darwin(tcflag_t f) {
  unsigned long d = 0;

  if (f & ISIG) d |= CRT_DARWIN_ISIG;
  if (f & ICANON) d |= CRT_DARWIN_ICANON;
  if (f & ECHO) d |= CRT_DARWIN_ECHO;
  if (f & ECHOE) d |= CRT_DARWIN_ECHOE;
  if (f & ECHOK) d |= CRT_DARWIN_ECHOK;
  if (f & ECHONL) d |= CRT_DARWIN_ECHONL;
  if (f & NOFLSH) d |= CRT_DARWIN_NOFLSH;
  if (f & TOSTOP) d |= CRT_DARWIN_TOSTOP;
  if (f & ECHOCTL) d |= CRT_DARWIN_ECHOCTL;
  if (f & ECHOPRT) d |= CRT_DARWIN_ECHOPRT;
  if (f & ECHOKE) d |= CRT_DARWIN_ECHOKE;
  if (f & FLUSHO) d |= CRT_DARWIN_FLUSHO;
  if (f & PENDIN) d |= CRT_DARWIN_PENDIN;
  if (f & IEXTEN) d |= CRT_DARWIN_IEXTEN;
  if (f & EXTPROC) d |= CRT_DARWIN_EXTPROC;
  /* XCASE (SysV-only legacy bit) has no Darwin equivalent -- dropped. */
  return d;
}

static tcflag_t darwin_lflag_to_bionic(unsigned long d) {
  tcflag_t f = 0;

  if (d & CRT_DARWIN_ISIG) f |= ISIG;
  if (d & CRT_DARWIN_ICANON) f |= ICANON;
  if (d & CRT_DARWIN_ECHO) f |= ECHO;
  if (d & CRT_DARWIN_ECHOE) f |= ECHOE;
  if (d & CRT_DARWIN_ECHOK) f |= ECHOK;
  if (d & CRT_DARWIN_ECHONL) f |= ECHONL;
  if (d & CRT_DARWIN_NOFLSH) f |= NOFLSH;
  if (d & CRT_DARWIN_TOSTOP) f |= TOSTOP;
  if (d & CRT_DARWIN_ECHOCTL) f |= ECHOCTL;
  if (d & CRT_DARWIN_ECHOPRT) f |= ECHOPRT;
  if (d & CRT_DARWIN_ECHOKE) f |= ECHOKE;
  if (d & CRT_DARWIN_FLUSHO) f |= FLUSHO;
  if (d & CRT_DARWIN_PENDIN) f |= PENDIN;
  if (d & CRT_DARWIN_IEXTEN) f |= IEXTEN;
  if (d & CRT_DARWIN_EXTPROC) f |= EXTPROC;
  /* ALTWERASE/NOKERNINFO (Darwin-only) have no Bionic equivalent -- both
   * dropped; CIGNORE lives in c_cflag on Darwin, not c_lflag, and is
   * likewise not translated (see bionic_cflag_to_darwin()'s comment). */
  return f;
}

static void termios_to_darwin(const struct termios* t, struct crt_darwin_termios* d) {
  int i;

  memset(d, 0, sizeof(*d));
  d->c_iflag = bionic_iflag_to_darwin(t->c_iflag);
  d->c_oflag = bionic_oflag_to_darwin(t->c_oflag);
  d->c_cflag = bionic_cflag_to_darwin(t->c_cflag);
  d->c_lflag = bionic_lflag_to_darwin(t->c_lflag);
  for (i = 0; i < 17; ++i) {
    int darwin_index = kBionicCcToDarwin[i];

    if (darwin_index >= 0) {
      d->c_cc[darwin_index] = t->c_cc[i];
    }
  }
  d->c_ispeed = bionic_speed_to_darwin(t->c_ispeed);
  d->c_ospeed = bionic_speed_to_darwin(t->c_ospeed);
}

static void darwin_to_termios(const struct crt_darwin_termios* d, struct termios* t) {
  int i;

  memset(t, 0, sizeof(*t));
  t->c_iflag = darwin_iflag_to_bionic(d->c_iflag);
  t->c_oflag = darwin_oflag_to_bionic(d->c_oflag);
  t->c_cflag = darwin_cflag_to_bionic(d->c_cflag);
  t->c_lflag = darwin_lflag_to_bionic(d->c_lflag);
  for (i = 0; i < CRT_DARWIN_NCCS; ++i) {
    int bionic_index = kDarwinCcToBionic[i];

    if (bionic_index >= 0) {
      t->c_cc[bionic_index] = d->c_cc[i];
    }
  }
  t->c_ispeed = darwin_speed_to_bionic(d->c_ispeed);
  t->c_ospeed = darwin_speed_to_bionic(d->c_ospeed);
}

long __crt_sys_ioctl(int fd, unsigned long request, void* arg);

long __crt_sys_tcgetattr(int fd, struct termios* termios_p) {
  struct crt_darwin_termios darwin_termios;
  long result = __crt_sys_ioctl(fd, CRT_DARWIN_TIOCGETA, &darwin_termios);

  if (result < 0) {
    return result;
  }
  darwin_to_termios(&darwin_termios, termios_p);
  return 0;
}

long __crt_sys_tcsetattr(int fd, int optional_actions, const struct termios* termios_p) {
  struct crt_darwin_termios darwin_termios;
  unsigned long request;

  switch (optional_actions) {
    case TCSANOW:
      request = CRT_DARWIN_TIOCSETA;
      break;
    case TCSADRAIN:
      request = CRT_DARWIN_TIOCSETAW;
      break;
    case TCSAFLUSH:
      request = CRT_DARWIN_TIOCSETAF;
      break;
    default:
      return -EINVAL;
  }
  termios_to_darwin(termios_p, &darwin_termios);
  return __crt_sys_ioctl(fd, request, &darwin_termios);
}

long __crt_sys_tcdrain(int fd) {
  return __crt_sys_ioctl(fd, CRT_DARWIN_TIOCDRAIN, 0);
}

/* Real BSD/Darwin tcflow() (this shared tty-layer lineage traces back
 * unchanged to 4.4BSD, per FreeBSD/NetBSD/OpenBSD's own long-published
 * lib/libc sources) dispatches to four distinct ioctls rather than
 * passing the TCOOFF/TCOON/TCIOFF/TCION action value straight through to
 * one generic ioctl -- matching the intent each request's own name/
 * comment documents in this SDK's <sys/ttycom.h> (TIOCSTART "start
 * output, like ^Q", TIOCSTOP "stop output, like ^S", TIOCIXON "internal
 * input VSTART", TIOCIXOFF "internal input VSTOP"). Carefully reasoned
 * from that evidence and the well-known, essentially unchanged-for-
 * decades BSD implementation pattern, not independently confirmed
 * against Apple's own closed-source Libc for this exact function body --
 * flagged the same honest way this project flags any raw syscall/ioctl
 * mapping it couldn't cross-check against real source. */
long __crt_sys_tcflow(int fd, int action) {
  switch (action) {
    case TCOOFF:
      return __crt_sys_ioctl(fd, CRT_DARWIN_TIOCSTOP, 0);
    case TCOON:
      return __crt_sys_ioctl(fd, CRT_DARWIN_TIOCSTART, 0);
    case TCIOFF:
      return __crt_sys_ioctl(fd, CRT_DARWIN_TIOCIXOFF, 0);
    case TCION:
      return __crt_sys_ioctl(fd, CRT_DARWIN_TIOCIXON, 0);
    default:
      return -EINVAL;
  }
}

long __crt_sys_tcflush(int fd, int queue_selector) {
  int darwin_selector;

  switch (queue_selector) {
    case TCIFLUSH:
      darwin_selector = CRT_DARWIN_TCIFLUSH;
      break;
    case TCOFLUSH:
      darwin_selector = CRT_DARWIN_TCOFLUSH;
      break;
    case TCIOFLUSH:
      darwin_selector = CRT_DARWIN_TCIOFLUSH;
      break;
    default:
      return -EINVAL;
  }
  return __crt_sys_ioctl(fd, CRT_DARWIN_TIOCFLUSH, &darwin_selector);
}

/* Real BSD/Darwin tcsendbreak(): set the break bit, hold it for four-
 * tenths of a second, then clear it -- matching this SDK's own man page
 * ("transmits a continuous stream of zero-valued bits for four-tenths of
 * a second ... duration parameter is ignored") and the well-known BSD
 * TIOCSBRK/TIOCCBRK pairing (same reasoning-and-honesty note as
 * __crt_sys_tcflow() above: matched against this host's man page and the
 * ioctl pair's own documented intent, not against Apple's closed-source
 * Libc directly). */
long __crt_sys_tcsendbreak(int fd, int duration) {
  struct timespec sleep_time;
  long result;

  (void)duration;
  result = __crt_sys_ioctl(fd, CRT_DARWIN_TIOCSBRK, 0);
  if (result < 0) {
    return result;
  }
  sleep_time.tv_sec = 0;
  sleep_time.tv_nsec = 400000000L;
  nanosleep(&sleep_time, 0);
  return __crt_sys_ioctl(fd, CRT_DARWIN_TIOCCBRK, 0);
}
