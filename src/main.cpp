/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "bpp_server/server.h"
#include "bpp_shared/platform/memory.h"
#include "bpp_shared/helpers/java/java_math.h"
#include "bpp_shared/logger/logger.h"
#include "bpp_shared/platform/platform.h"
#include "logger.h"
#include "bpp_shared/platform/platform_label.h"
#include "quick_arg_parser.hpp"
#include "version.h"
#include <atomic>
#include <csignal>
#include <fstream>
#include <numeric_structs.h>
#include <sstream>

#ifndef BUILD_SERVER
#include "bpp_client/client.h"
#else
#if defined(ONLINE_MODE_AUTHENTICATION) || defined(BETACRAFT_HEARTBEAT)
#include "internet/curl_runtime.h"
#endif
#endif

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#endif

#include "bpp_utilities/utilities.h"
#include <format>
#include <thread>
#if defined(BPP_HAS_POSIX_SIGNALS)
#include <pthread.h>
#include <signal.h>
#endif

#include "nbt/nbt_textifier.h" // FOR TESTING

Server* server;
std::atomic<bool> shutdownRequested{ false };

#if defined(_WIN32) || defined(_WIN64)
BOOL WINAPI consoleCtrlHandler(DWORD dwCtrlType) {
	switch (dwCtrlType) {
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_SHUTDOWN_EVENT:
	case CTRL_LOGOFF_EVENT:
		shutdownRequested.store(true);
		// Block so the OS doesn't kill us before the main thread finishes saving
		while (shutdownRequested.load())
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		return TRUE;
	default:
		return FALSE;
	}
}
#endif

// Fall back to being a server if neither are defined
#if !defined(BUILD_SERVER) && !defined(BUILD_CLIENT)
#define BUILD_SERVER
#endif

static void SignalHandler(int /*sig*/) {
	shutdownRequested.store(true);
}

#if defined(BPP_HAS_POSIX_SIGNALS)
static void BlockStopSignals() {
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGTERM);
	pthread_sigmask(SIG_BLOCK, &set, nullptr);
}

// Dedicated waiter so SIGINT/SIGTERM cannot be swallowed by libcurl/OpenSSL
// handlers or by libc++ restarting sleep_until() on EINTR (musl SA_RESTART).
static void SignalWatchThread() {
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGTERM);
	int sig = 0;
	if (sigwait(&set, &sig) == 0)
		shutdownRequested.store(true);
}
#endif

static void InstallProcessSignalHandlers() {
#if !defined(BPP_HAS_POSIX_SIGNALS)
	std::signal(SIGINT, SignalHandler);
	std::signal(SIGTERM, SignalHandler);
#else
	struct sigaction sa{};
	sa.sa_handler = SignalHandler;
	sigemptyset(&sa.sa_mask);
	// musl's signal() sets SA_RESTART, which makes a later libcurl/OpenSSL
	// connect ignore SIGTERM. We want the tick loop to wake and shut down.
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, nullptr);
	sigaction(SIGTERM, &sa, nullptr);
#endif
#ifdef SIGPIPE
	std::signal(SIGPIPE, SIG_IGN);
#endif
}

struct Args : MainArguments<Args> {
	inline static const std::string Version() noexcept {
		return std::string(PROJECT_FULL_VERSION_LABEL);
	}
	uint16_t port = Option("port", 'p', "Port the server will run on (25565)") = 25565;
	int32_t maxPlayers = Option("max_players", '\0',
	                            "Maximum number of players. Anything less than 0 removes the cap (-1)") = -1;
	bool enableWhitelist = (Option("whitelist", 'w', "Enables usage of the whitelist") = false);
	int64_t seed = Option("seed", 's', "Overwrites the worlds current seed") = 0;
	bool disablePortals = (Option("no_portals", '\0', "Disables Portal-travel") = false);
	bool forceNetherSpawn = (Option("force_nether_spawn", '\0', "Makes players spawn in the Nether") = false);
	uint32_t pregenRadius = Option("pregen_radius", '\0',
	                               "Generates chunks around 0,0 until the desired radius is met") = 5;
	uint32_t chunkRenderRadius = Option(
	    "chunk_render_radius", '\0',
	    "Radius within which chunks are rendered for clients. On Vanilla clients this caps out at about 13 Chunks") = 5;
	uint32_t chunkGenRadius = Option("chunk_gen_radius", '\0', "Radius within which chunks are generated") = 5;
	uint32_t chunkTickRadius = Option("chunk_tick_radius", '\0', "Radius within which chunks are randomly ticked") = 5;
	uint32_t entityRenderRadius = Option("entity_render_radius", '\0', "Radius within which entities are shown") = 5;
	uint32_t entityTickRadius = Option("entity_tick_radius", '\0', "Radius within which entities are ticked") = 5;
};

#ifdef CRASH_LOGGING
#include "CrashCatch.hpp"
void InitCrashHandler(std::string _platformString) {
	CrashCatch::Config config{ .dumpFolder = "./",
		                       .enableTextLog = true,
		                       .autoTimestamp = true,
#if defined(BUILD_SERVER)
		                       .showCrashDialog = false,
#else
		                       .showCrashDialog = true,
#endif
		                       .appVersion = std::string(PROJECT_VERSION_FULL_STRING),
		                       .buildConfig = _platformString };
	config.onCrash = [](const CrashCatch::CrashContext& _ctx) {
		Logger::Error << "========== CRASH ==========\n";
		Logger::Error << "Signal/Code: " << _ctx.signalOrCode << "\n";

		// Pull the report into memory so it can be both printed and handed to addons.
		std::string report;
		std::string reportPath;
		if (!_ctx.logFilePath.empty()) {
			reportPath = _ctx.logFilePath.string();
			Logger::Error << "Crash report: " << reportPath << "\n";

			std::ifstream file(_ctx.logFilePath);
			if (file) {
				std::ostringstream buffer;
				buffer << file.rdbuf();
				report = buffer.str();
				Logger::Error << report << "\n";
			} else {
				Logger::Error << "Failed to open crash report file!\n";
			}
		}

		try {
			if (server) {
				std::string versionString(PROJECT_VERSION_FULL_STRING);
				const bp_crash_event event{ .crash_log = report.data(),
					                        .crash_log_path = reportPath.data(),
					                        .version_string = versionString.data() };
				server->GetAddonManager().Broadcast(&bp_addon_events::crash, event);
			}
		} catch (...) {
			Logger::Error << "An addon threw while handling the crash event!\n";
		}
	};
	CrashCatch::initialize(config);
}
#endif

int main(int /*argc*/, char** /*argv*/) {
	// Platform-specific Init
	Platform::Init();
	Logger::Init();
	std::string platformString = std::format("{} ({}, {})", PLATFORM_NAME, BUILD_MODE, ARCH_NAME);
#ifdef CRASH_LOGGING
	InitCrashHandler(platformString);
#endif
#if defined(BPP_HAS_POSIX_SIGNALS)
	// Block before any other threads so they inherit this mask; sigwait in
	// SignalWatchThread is then the only consumer of SIGINT/SIGTERM.
	BlockStopSignals();
	std::thread(SignalWatchThread).detach();
#endif
	InstallProcessSignalHandlers();
	// Parse CLI Args
	//Args args{ { argc, argv } };
	// Init the sine table
	MathHelper::InitSinTable();
	// We're ready to roll
	Logger::Info << "Running on " << platformString << "\n";
#if defined(_WIN32) || defined(_WIN64)
	SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);
#endif

#ifdef BUILD_SERVER
#if defined(ONLINE_MODE_AUTHENTICATION) || defined(BETACRAFT_HEARTBEAT)
	// Initialize libcurl before the server starts its thread pools. First-login
	// curl_easy_init() would otherwise do this unsafely and can block SIGTERM.
	if (!CurlRuntimeInit())
		Logger::Warning << "libcurl global init failed; online-mode auth and Betacraft heartbeat may not work.\n";
	InstallProcessSignalHandlers();
#if defined(BPP_HAS_POSIX_SIGNALS)
	BlockStopSignals();
#endif
#endif
	Server serv;
	//if (args.enableWhitelist)
	//	serv.SetWhitelistEnabled(true, false);
	server = &serv;
	server->Run();
#if defined(ONLINE_MODE_AUTHENTICATION) || defined(BETACRAFT_HEARTBEAT)
	CurlRuntimeCleanup();
#endif
#endif
#ifdef BUILD_CLIENT
	Client client;
	client.Run();
#endif
	Logger::Close();
	// Platform-specific Close
	Platform::Shutdown();
	return 0;
}