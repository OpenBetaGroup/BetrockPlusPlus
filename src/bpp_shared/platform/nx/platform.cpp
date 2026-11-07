/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "../platform.h"

#if defined(__SWITCH__)
#include "../homebrew/system_pump.h"
#include <chrono>
#include <cstdio>
#include <switch.h>
#include <thread>

namespace {
Platform::Homebrew::SystemPump pump;
PadState pad;
bool socketsUp = false;

// One iteration of the per-frame OS service
bool ServiceFrame() {
	if (!appletMainLoop())
		return false; // HOME menu "close"
	padUpdate(&pad);
	if (padGetButtonsDown(&pad) & HidNpadButton_Plus)
		return false;
	consoleUpdate(nullptr);
	// appletMainLoop() doesn't block, so pace to roughly one frame ourselves
	std::this_thread::sleep_for(std::chrono::milliseconds(16));
	return true;
}
} // namespace

bool Platform::Init() {
	consoleInit(nullptr);
	padConfigureInput(1, HidNpadStyleSet_NpadStandard);
	padInitializeDefault(&pad);

	// From here on the OS is serviced regardless of what the server is busy doing
	pump.Start(ServiceFrame);
	Homebrew::InstallTerminateReporter(pump);
	std::printf("Betrock++ - press + to stop the server\n");

	// Config, logs and worlds are relative paths; keep them in one known folder
	if (!Homebrew::EnterDataDirectory("sdmc:/switch/BetrockPlusPlus"))
		std::printf("Couldn't open sdmc:/switch/BetrockPlusPlus, using the launch folder\n");

	socketsUp = R_SUCCEEDED(socketInitializeDefault());
	if (!socketsUp)
		std::printf("socketInitializeDefault failed (is the console online?)\n");
	return socketsUp;
}

void Platform::Shutdown() {
	pump.Stop();
	if (socketsUp) {
		socketExit();
		socketsUp = false;
	}
	consoleExit(nullptr);
}
#endif
