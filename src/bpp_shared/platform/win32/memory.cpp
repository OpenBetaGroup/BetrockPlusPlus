/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../memory.h"

#if defined(_WIN32)

// The K32* functions are only declared when _WIN32_WINNT targets Vista (0x0600) or later.
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#ifndef WINVER
#define WINVER 0x0600
#endif
#define PSAPI_VERSION 2
#define WIN32_LEAN_AND_MEAN
// NOTE: Due to psapi.h relying on macros defined in windows.h,
// we're not able to reorder these includes. Why psapi.h doesn't just include
// windows.h by itself is a question only the madmen at Microsoft can answer.
// TL;DR:
// For the love of god, don't let the auto-formatter switch these around!
// clang-format off
#include <windows.h>
#include <psapi.h>
// clang-format on
#include <vector>

double GetMemoryUsage(const MemoryUnit _unit) {
	PROCESS_MEMORY_COUNTERS pmc{};
	pmc.cb = sizeof(pmc);

	if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
		return 0.0;

	// WorkingSetSize is the closest Windows analogue to Linux RSS.
	return static_cast<double>(pmc.WorkingSetSize) / BytesPerUnit(_unit);
}

double GetActiveMemoryUsage(const MemoryUnit _unit) {
	DWORD heapCount = GetProcessHeaps(0, nullptr);
	if (heapCount == 0)
		return 0.0;

	std::vector<HANDLE> heaps(heapCount);
	heapCount = GetProcessHeaps(heapCount, heaps.data());

	size_t inUseBytes = 0;
	for (DWORD i = 0; i < heapCount; ++i) {
		if (!HeapLock(heaps[i]))
			continue;

		PROCESS_HEAP_ENTRY entry{};
		while (HeapWalk(heaps[i], &entry)) {
			if (entry.wFlags & PROCESS_HEAP_ENTRY_BUSY)
				inUseBytes += entry.cbData;
		}

		HeapUnlock(heaps[i]);
	}
	return static_cast<double>(inUseBytes) / BytesPerUnit(_unit);
}

// Get the current memory page size in Bytes (Cached after first use)
size_t GetPageSize() {
	static const size_t pageSize = []() -> size_t {
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		return static_cast<size_t>(si.dwPageSize);
	}();
	return pageSize;
}

void TrimMemory() {
	DWORD heapCount = GetProcessHeaps(0, nullptr);
	if (heapCount != 0) {
		std::vector<HANDLE> heaps(heapCount);
		heapCount = GetProcessHeaps(heapCount, heaps.data());
		for (DWORD i = 0; i < heapCount; ++i)
			HeapCompact(heaps[i], 0);
	}
	EmptyWorkingSet(GetCurrentProcess());
}
#endif
