/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../hardware_info.h"

#if defined(__3DS__)
#include <3ds.h>

HardwareInfo QueryHardwareInfo() {
	HardwareInfo info;
	bool isNew3DS = false;
	if (R_FAILED(APT_CheckNew3DS(&isNew3DS)))
		isNew3DS = false;

	info.os = "Horizon OS";
	info.kernel = "libctru";
	info.cpuModel = isNew3DS ? "ARM11 MPCore (New 3DS)" : "ARM11 MPCore";
	info.threads = isNew3DS ? 4 : 2;
	info.cpuMhz = isNew3DS ? 804.0 : 268.0;
	// What the application is allowed to use, not the physical RAM of the console
	info.memoryTotalBytes = osGetMemRegionSize(MEMREGION_APPLICATION);
	info.swapTotalBytes = 0;
	return info;
}
#endif
