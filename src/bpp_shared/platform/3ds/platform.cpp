/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "../platform.h"

#if defined(__3DS__)
#include "../homebrew/system_pump.h"
#include <3ds.h>
#include <cstdio>
#include <malloc.h>

namespace {
// Recommended defaults from libctru's own examples. The SOC service needs a
// page-aligned buffer to use as its transfer area; 128KiB is the minimum
// documented, 1MiB gives some headroom for many concurrent connections.
constexpr size_t SOC_ALIGNMENT = 0x1000;
constexpr size_t SOC_BUFFER_SIZE = 0x100000;
u32* socBuffer = nullptr;

Platform::Homebrew::SystemPump pump;

// One iteration of the per-frame OS service. gspWaitForVBlank() blocks until
// the next frame, which is what paces this loop.
bool ServiceFrame() {
	if (!aptMainLoop())
		return false; // HOME menu "close"
	hidScanInput();
	if (hidKeysDown() & KEY_START)
		return false;
	gfxFlushBuffers();
	gfxSwapBuffers();
	gspWaitForVBlank();
	return true;
}
} // namespace

bool Platform::Init() {
	gfxInitDefault();
	consoleInit(GFX_TOP, nullptr);
	// New 3DS: 804MHz + extra L2 cache. Harmless no-op on the original models.
	osSetSpeedupEnable(true);

	// From here on the OS is serviced regardless of what the server is busy doing
	pump.Start(ServiceFrame);
	Homebrew::InstallTerminateReporter(pump);
	std::printf("Betrock++ - press START to stop the server\n");

	// Config, logs and worlds are relative paths; keep them in one known folder
	if (!Homebrew::EnterDataDirectory("sdmc:/3ds/BetrockPlusPlus"))
		std::printf("Couldn't open sdmc:/3ds/BetrockPlusPlus, using the launch folder\n");

	socBuffer = static_cast<u32*>(memalign(SOC_ALIGNMENT, SOC_BUFFER_SIZE));
	if (!socBuffer) {
		std::printf("Failed to allocate the SOC buffer\n");
		return false;
	}
	if (R_FAILED(socInit(socBuffer, SOC_BUFFER_SIZE))) {
		std::printf("socInit failed (is Wi-Fi on?)\n");
		free(socBuffer);
		socBuffer = nullptr;
		return false;
	}
	return true;
}

void Platform::Shutdown() {
	pump.Stop();
	if (socBuffer) {
		socExit();
		free(socBuffer);
		socBuffer = nullptr;
	}
	gfxExit();
}
#endif
