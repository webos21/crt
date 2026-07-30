#ifndef CRT_TARGETCONDITIONALS_H
#define CRT_TARGETCONDITIONALS_H

/*
 * Minimal CRT-owned compatibility surface for upstream sources that check
 * Apple target conditionals when built on a macOS host. This intentionally
 * avoids including the host SDK header through the CRT sysroot wrapper.
 */

#define TARGET_OS_MAC 1
#define TARGET_OS_OSX 1
#define TARGET_OS_UNIX 0
#define TARGET_OS_LINUX 0
#define TARGET_OS_WINDOWS 0
#define TARGET_OS_WIN32 0

#define TARGET_OS_IPHONE 0
#define TARGET_OS_IOS 0
#define TARGET_OS_TV 0
#define TARGET_OS_WATCH 0
#define TARGET_OS_VISION 0
#define TARGET_OS_BRIDGE 0
#define TARGET_OS_SIMULATOR 0
#define TARGET_OS_MACCATALYST 0
#define TARGET_OS_DRIVERKIT 0

#define TARGET_OS_EMBEDDED 0
#define TARGET_IPHONE_SIMULATOR 0
#define TARGET_OS_NANO 0

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define TARGET_RT_BIG_ENDIAN 1
#define TARGET_RT_LITTLE_ENDIAN 0
#else
#define TARGET_RT_BIG_ENDIAN 0
#define TARGET_RT_LITTLE_ENDIAN 1
#endif

#if defined(__LP64__) || defined(_WIN64)
#define TARGET_RT_64_BIT 1
#else
#define TARGET_RT_64_BIT 0
#endif

#define TARGET_RT_MAC_CFM 0
#define TARGET_RT_MAC_MACHO 1

#endif
