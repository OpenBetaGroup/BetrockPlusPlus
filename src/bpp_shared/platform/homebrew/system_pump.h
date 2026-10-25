/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

// Shared helpers for some consoles, such as the Nintendo 3DS and Switch homebrew targets.
//
// Both consoles expect the application to service the OS (applet/APT events,
// the frame buffer) every frame, and kill or hang it if it doesn't. Our server
// loop is a fixed-rate tick that can also block for many seconds (spawn chunk
// generation, saving), so it can't be the thing that does this.
// SystemPump runs the per-frame service call on its own thread instead, which
// keeps the server code free of any console specific calls.

#pragma once

#include <atomic>
#include <chrono>
#include <cstdio>
#include <exception>
#include <functional>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

// Defined in main.cpp. Setting it makes Server::Run() save the world and return.
extern std::atomic<bool> shutdownRequested;

namespace Platform::Homebrew {

class SystemPump {
public:
	// _service runs in a loop on the pump thread. It has to pace itself
	// (vblank wait or a short sleep) and return false once the OS wants us gone
	// (HOME menu close / exit button), which then requests a clean server shutdown.
	void Start(std::function<bool()> _service) {
		if (thread.joinable())
			return;
		running.store(true);
		alive.store(true);
		thread = std::thread([this, service = std::move(_service)] {
			while (running.load()) {
				if (!service()) {
					shutdownRequested.store(true);
					break;
				}
			}
			alive.store(false);
		});
	}

	void Stop() {
		running.store(false);
		if (thread.joinable())
			thread.join();
	}

	// False once the OS has asked us to quit (or after Stop())
	bool Alive() const {
		return alive.load();
	}

private:
	std::thread thread;
	std::atomic<bool> running{ false };
	std::atomic<bool> alive{ false };
};

// mkdir -p _dir, then chdir into it. The config, logs and world are all
// relative paths, so this pins them to one known folder on the SD card instead
// of whatever the launcher happened to leave as the working directory.
inline bool EnterDataDirectory(const std::string& _dir) {
	// Create each component; the "sdmc:" device prefix has to be skipped
	size_t start = _dir.find(":/");
	start = (start == std::string::npos) ? 0 : start + 2;
	for (size_t i = start; i <= _dir.size(); ++i) {
		if (i == _dir.size() || _dir[i] == '/') {
			if (i > start)
				mkdir(_dir.substr(0, i).c_str(), 0777); // already existing is fine
		}
	}
	return chdir(_dir.c_str()) == 0;
}

// Without a terminal an uncaught exception just kills the app and the reason
// is lost. Print it and keep the pump alive until the user leaves via HOME.
inline void InstallTerminateReporter(SystemPump& _pump) {
	static SystemPump* pump = nullptr;
	pump = &_pump;
	std::set_terminate([] {
		std::string message = "unknown exception";
		if (std::exception_ptr current = std::current_exception()) {
			try {
				std::rethrow_exception(current);
			} catch (const std::exception& e) {
				message = e.what();
			} catch (...) {
			}
		}
		std::printf("\nFATAL: %s\nClose the app from the HOME menu to exit.\n", message.c_str());
		std::fflush(stdout);
		while (pump && pump->Alive())
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		std::_Exit(1);
	});
}

} // namespace Platform::Homebrew
