/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../memory.h"

#if defined(__3DS__)
#include <3ds.h>
#include <malloc.h>

// The 3DS has no process RSS to ask for, so "memory usage" is what newlib's
// heap has taken from the OS so far (arena) vs. what is actually handed out.
double GetMemoryUsage(const MemoryUnit _unit) {
	struct mallinfo mi = mallinfo();
	return static_cast<double>(mi.arena) / BytesPerUnit(_unit);
}

double GetActiveMemoryUsage(const MemoryUnit _unit) {
	struct mallinfo mi = mallinfo();
	return static_cast<double>(mi.uordblks) / BytesPerUnit(_unit);
}

void TrimMemory() {}
#endif
