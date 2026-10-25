/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

// This is used for platform-specific functionality,
// so it can be capsuled away more cleanly

#pragma once

#include "platform_caps.h"

namespace Platform {
#if defined(BPP_PLATFORM_CONSOLE)
// Implemented in the platform-specific platform.cpp files
// Run at program start. Brings up the console/gfx, keeps the OS service loop
// alive on a background thread, sets up sockets and enters the data folder.
bool Init();
// Run at the end of the program. Stops the service loop and tears everything down.
void Shutdown();
#else
// This is just the generic fallback for any platforms that don't make use of this
// Run at program start. Used for initializing network stack, etc.
inline bool Init() { return true; }
// Run at the end of the program. Can be used for clean-up of network stack, etc.
inline void Shutdown() {}
#endif
}
