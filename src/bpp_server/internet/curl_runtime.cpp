/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#if defined(ONLINE_MODE_AUTHENTICATION) || defined(BETACRAFT_HEARTBEAT)
#include "curl_runtime.h"

#include <atomic>
#include <csignal>
#include <curl/curl.h>
#if defined(__linux__) || defined(__APPLE__) || defined(__HAIKU__) || defined(PS2)
#include <pthread.h>
#include <signal.h>
#endif

extern std::atomic<bool> shutdownRequested;

static std::atomic<bool> curlInitialized{ false };

static void StopSignalHandler(int /*sig*/) {
	shutdownRequested.store(true);
}

void Curl::RestoreStopSignals() {
#if defined(_WIN32) || defined(_WIN64)
	std::signal(SIGINT, StopSignalHandler);
	std::signal(SIGTERM, StopSignalHandler);
#else
	struct sigaction sa{};
	sa.sa_handler = StopSignalHandler;
	sigemptyset(&sa.sa_mask);
	// No SA_RESTART: a blocked connect/wait must return so the tick loop
	// can observe shutdownRequested.
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, nullptr);
	sigaction(SIGTERM, &sa, nullptr);
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGTERM);
	// Keep these blocked so the sigwait thread in main() receives them.
	// Unblocking here let libcurl/OpenSSL swallow SIGTERM on musl.
	pthread_sigmask(SIG_BLOCK, &set, nullptr);
#ifdef SIGPIPE
	std::signal(SIGPIPE, SIG_IGN);
#endif
#endif
}

// Note: MUST happen before any other threads exist, otherwise this can block all other signals on Musl
bool Curl::Init() {
	if (curlInitialized.exchange(true)) {
		Curl::RestoreStopSignals();
		return true;
	}
	if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
		curlInitialized.store(false);
		return false;
	}
	Curl::RestoreStopSignals();
	return true;
}

void Curl::Cleanup() {
	if (!curlInitialized.exchange(false))
		return;
	curl_global_cleanup();
}
#endif
