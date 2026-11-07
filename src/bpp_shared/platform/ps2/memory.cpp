/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../memory.h"

#if defined(PS2)
#include <malloc.h>

double GetMemoryUsage(const MemoryUnit _unit) {
	struct mallinfo mi = mallinfo();
	return static_cast<double>(mi.arena) / BytesPerUnit(_unit);
}

double GetActiveMemoryUsage(const MemoryUnit _unit) {
	struct mallinfo mi = mallinfo();
	return static_cast<double>(mi.uordblks) / BytesPerUnit(_unit);
}

// These targets allocate in 4 KiB units
size_t GetPageSize() {
	return 4096;
}

void TrimMemory() {}
#endif
