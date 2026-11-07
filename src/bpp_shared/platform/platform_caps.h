/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

// Capability flags for the OS/libc features that differ between targets.
// Prefer testing one of these over spelling out a list of platforms at the
// call site, so a new target only needs to be taught about here.

#pragma once

// Nintendo Switch (libnx) and Nintendo 3DS (libctru) homebrew
#if defined(__3DS__) || defined(__SWITCH__)
#define BPP_PLATFORM_CONSOLE 1
#endif

// <sys/socket.h>, <netinet/in.h>, fcntl(O_NONBLOCK), ...
#if defined(__linux__) || defined(__APPLE__) || defined(__HAIKU__) || defined(PS2) || defined(BPP_PLATFORM_CONSOLE)
#define BPP_HAS_POSIX_SOCKETS 1
#endif

// sigaction / sigwait / pthread_sigmask. Without them we fall back to std::signal.
#if !defined(_WIN32) && !defined(_WIN64) && !defined(PS2) && !defined(BPP_PLATFORM_CONSOLE)
#define BPP_HAS_POSIX_SIGNALS 1
#endif

// flock() advisory locking (session.lock)
#if !defined(_WIN32) && !defined(_WIN64) && !defined(BPP_PLATFORM_CONSOLE)
#define BPP_HAS_FLOCK 1
#endif

// IPPROTO_TCP / TCP_NODELAY (libctru has no <netinet/tcp.h> option support)
#if !defined(__3DS__)
#define BPP_HAS_TCP_NODELAY 1
#endif

// SO_RCVTIMEO. Our sockets are non-blocking anyway, so the consoles skip it.
#if !defined(BPP_PLATFORM_CONSOLE)
#define BPP_HAS_SO_RCVTIMEO 1
#endif

// SO_SNDTIMEO. Same story as SO_RCVTIMEO: not available in the console SDKs.
#if !defined(BPP_PLATFORM_CONSOLE)
#define BPP_HAS_SO_SNDTIMEO 1
#endif

// listen() backlog. SOMAXCONN isn't reliably defined by the console SDKs.
#if defined(BPP_PLATFORM_CONSOLE)
#define BPP_LISTEN_BACKLOG 8
#else
#define BPP_LISTEN_BACKLOG SOMAXCONN
#endif

// No terminal/line editor (replxx) and no dlopen() on the consoles
#if defined(BPP_PLATFORM_CONSOLE)
#define BPP_HEADLESS_CONSOLE 1
#define BPP_NO_DYNAMIC_ADDONS 1
#endif

// Default view distance (mostly figured out experimentally)
#if defined (__3DS__) || defined(__wii__)
#define BPP_DEFAULT_VIEW_DISTANCE 3
#elif defined (__NDS__)
#define BPP_DEFAULT_VIEW_DISTANCE 2
#else
#define BPP_DEFAULT_VIEW_DISTANCE 10
#endif
