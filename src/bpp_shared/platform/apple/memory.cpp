/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../memory.h"

#if defined(__APPLE__)

#include <mach/mach.h>
#include <malloc/malloc.h>
#include <unistd.h>

double GetMemoryUsage(const MemoryUnit _unit) {
	mach_task_basic_info_data_t info{};
	mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;

	if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS)
		return 0.0;

	// resident_size is macOS's RSS equivalent.
	return static_cast<double>(info.resident_size) / BytesPerUnit(_unit);
}

double GetActiveMemoryUsage(const MemoryUnit _unit) {
	vm_address_t* zones = nullptr;
	unsigned zoneCount = 0;

	if (malloc_get_all_zones(mach_task_self(), nullptr, &zones, &zoneCount) != KERN_SUCCESS)
		return GetMemoryUsage(_unit); // fall back to RSS if zone enumeration fails

	size_t inUseBytes = 0;
	for (unsigned i = 0; i < zoneCount; ++i) {
		auto* zone = reinterpret_cast<malloc_zone_t*>(zones[i]);
		if (!zone)
			continue;

		malloc_statistics_t stats{};
		malloc_zone_statistics(zone, &stats);
		inUseBytes += stats.size_in_use;
	}
	return static_cast<double>(inUseBytes) / BytesPerUnit(_unit);
}

// Get the current memory page size in Bytes (Cached after first use)
size_t GetPageSize() {
	static const size_t pageSize = []() -> size_t {
		const long size = sysconf(_SC_PAGESIZE);
		return size > 0 ? static_cast<size_t>(size) : 4096;
	}();
	return pageSize;
}

// Applies pressure relief to every zone, prompting the allocator to release
// freed pages back to the OS.
void TrimMemory() {
	vm_address_t* zones = nullptr;
	unsigned zoneCount = 0;

	if (malloc_get_all_zones(mach_task_self(), nullptr, &zones, &zoneCount) != KERN_SUCCESS)
		return;

	for (unsigned i = 0; i < zoneCount; ++i) {
		auto* zone = reinterpret_cast<malloc_zone_t*>(zones[i]);
		if (zone)
			malloc_zone_pressure_relief(zone, 0);
	}
}
#endif
