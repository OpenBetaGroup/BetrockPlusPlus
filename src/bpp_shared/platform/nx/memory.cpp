/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../memory.h"

#if defined(__SWITCH__)
#include <malloc.h>
#include <switch.h>

// Memory the kernel has handed to this process
double GetMemoryUsage(const MemoryUnit _unit) {
	u64 used = 0;
	if (R_FAILED(svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0)))
		return 0.0;
	return static_cast<double>(used) / BytesPerUnit(_unit);
}

// What the allocator considers "in use"
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
