/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../hardware_info.h"

#if defined(__SWITCH__)
#include <switch.h>

HardwareInfo QueryHardwareInfo() {
	HardwareInfo info;
	info.os = "Horizon OS";
	info.kernel = "libnx";
	info.cpuModel = "ARM Cortex-A57";
	info.threads = 3; // cores available to applications
	info.cpuMhz = 1020.0;
	u64 total = 0;
	if (R_SUCCEEDED(svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0)))
		info.memoryTotalBytes = total; // what this process may use, not the console's RAM
	info.swapTotalBytes = 0;
	return info;
}
#endif
