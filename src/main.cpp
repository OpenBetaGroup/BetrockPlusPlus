/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#if !defined(BUILD_SERVER) && !defined(BUILD_CLIENT)
#define BUILD_SERVER
#endif

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <format>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#endif

#if defined(BPP_HAS_POSIX_SIGNALS)
#include <pthread.h>
#include <signal.h>
#endif

#include "quick_arg_parser.hpp"
#ifdef CRASH_LOGGING
#include "CrashCatch.hpp"
#endif

// Shared Libs
#include "bpp_shared/helpers/java/java_math.h"
#include "bpp_shared/logger/logger.h"
#include "bpp_shared/platform/memory.h"
#include "bpp_shared/platform/platform.h"
#include "bpp_shared/platform/platform_label.h"
#include "bpp_utilities/utilities.h"
#include "version.h"
#include <numeric_structs.h>

// Server includes
#ifdef BUILD_SERVER
#if defined(ONLINE_MODE_AUTHENTICATION) || defined(BETACRAFT_HEARTBEAT)
#include "internet/curl_runtime.h"
#endif
#include "bpp_server/server.h"
#endif

// Client includes
#ifdef BUILD_CLIENT
#include "bpp_client/client.h"
#endif

#include "nbt/nbt_textifier.h" // FOR TESTING

// Globals
std::atomic<bool> shutdownRequested{ false };
#ifdef BUILD_SERVER
static Server* server = nullptr;
#endif

// Command line arguments
// TODO: Actually make use of these!!!
struct Args : MainArguments<Args> {
	inline static const std::string Version() noexcept {
		return std::string(PROJECT_FULL_VERSION_LABEL);
	}

	// --- Network / players ---
	uint16_t port = Option("port", 'p', "Port the server will run on (25565)") = 25565;
	int32_t maxPlayers = Option("max_players", '\0',
	                            "Maximum number of players. Anything less than 0 removes the cap (-1)") = -1;
	bool enableWhitelist = (Option("whitelist", 'w', "Enables usage of the whitelist") = false);

	// --- World ---
	int64_t seed = Option("seed", 's', "Overwrites the worlds current seed") = 0;
	bool disablePortals = (Option("no_portals", '\0', "Disables Portal-travel") = false);
	bool forceNetherSpawn = (Option("force_nether_spawn", '\0', "Makes players spawn in the Nether") = false);
	uint32_t pregenRadius = Option("pregen_radius", '\0',
	                               "Generates chunks around 0,0 until the desired radius is met") = 5;

	// --- Radii ---
	uint32_t chunkRenderRadius = Option(
	    "chunk_render_radius", '\0',
	    "Radius within which chunks are rendered for clients. On Vanilla clients this caps out at about 13 Chunks") = 5;
	uint32_t chunkGenRadius = Option("chunk_gen_radius", '\0', "Radius within which chunks are generated") = 5;
	uint32_t chunkTickRadius = Option("chunk_tick_radius", '\0', "Radius within which chunks are randomly ticked") = 5;
	uint32_t entityRenderRadius = Option("entity_render_radius", '\0', "Radius within which entities are shown") = 5;
	uint32_t entityTickRadius = Option("entity_tick_radius", '\0', "Radius within which entities are ticked") = 5;
};

// Shutdown / Signal handling
static void SignalHandler(int /*sig*/) {
	shutdownRequested.store(true);
}

// Windows stuff
#if defined(_WIN32) || defined(_WIN64)
static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
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

#if defined(BPP_HAS_POSIX_SIGNALS)
static void BlockStopSignals() {
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGTERM);
	pthread_sigmask(SIG_BLOCK, &set, nullptr);
}

// Dedicated waiter so SIGINT/SIGTERM cannot be swallowed by libcurl/OpenSSL
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
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, nullptr);
	sigaction(SIGTERM, &sa, nullptr);
#endif
#ifdef SIGPIPE
	std::signal(SIGPIPE, SIG_IGN);
#endif
}

// One-stop setup, called from main().
static void InitShutdownHandling() {
#if defined(BPP_HAS_POSIX_SIGNALS)
	BlockStopSignals();
	std::thread(SignalWatchThread).detach();
#endif
	InstallProcessSignalHandlers();
#if defined(_WIN32) || defined(_WIN64)
	SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
#endif
}

// Crash logging
#ifdef CRASH_LOGGING
static void InitCrashHandler(const std::string& platformString) {
	CrashCatch::Config config{ .dumpFolder = "./",
		                       .enableTextLog = true,
		                       .autoTimestamp = true,
#if defined(BUILD_SERVER)
		                       .showCrashDialog = false,
#else
		                       .showCrashDialog = true,
#endif
		                       .appVersion = std::string(PROJECT_VERSION_FULL_STRING),
		                       .buildConfig = platformString };

	config.onCrash = [](const CrashCatch::CrashContext& ctx) {
		Logger::Error << "========== CRASH ==========\n";
		Logger::Error << "Signal/Code: " << ctx.signalOrCode << "\n";

		// Pull the report into memory so it can be both printed and handed to addons.
		std::string report;
		std::string reportPath;
		if (!ctx.logFilePath.empty()) {
			reportPath = ctx.logFilePath.string();
			Logger::Error << "Crash report: " << reportPath << "\n";

			std::ifstream file(ctx.logFilePath);
			if (file) {
				std::ostringstream buffer;
				buffer << file.rdbuf();
				report = buffer.str();
				Logger::Error << report << "\n";
			} else {
				Logger::Error << "Failed to open crash report file!\n";
			}
		}

#ifdef BUILD_SERVER
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
#endif
	};

	CrashCatch::initialize(config);
}
#endif

// Entry points
#ifdef BUILD_SERVER
static void RunServer() {
#if defined(ONLINE_MODE_AUTHENTICATION) || defined(BETACRAFT_HEARTBEAT)
	// Initialize libcurl before the server starts its thread pools. First-login
	// curl_easy_init() would otherwise do this unsafely and can block SIGTERM.
	if (!CurlRuntimeInit())
		Logger::Warning << "libcurl global init failed; online-mode auth and Betacraft heartbeat may not work.\n";
	// libcurl/OpenSSL may have replaced our handlers; put them back.
	InstallProcessSignalHandlers();
#if defined(BPP_HAS_POSIX_SIGNALS)
	BlockStopSignals();
#endif
#endif

	Server serv;
	// if (args.enableWhitelist)
	//	serv.SetWhitelistEnabled(true, false);
	server = &serv;
	serv.Run();
	server = nullptr; // don't leave a dangling pointer for the crash handler

#if defined(ONLINE_MODE_AUTHENTICATION) || defined(BETACRAFT_HEARTBEAT)
	CurlRuntimeCleanup();
#endif
}
#endif

#ifdef BUILD_CLIENT
static void RunClient() {
	Client client;
	client.Run();
}
#endif

int main(int /*argc*/, char** /*argv*/) {
	Platform::Init();
	Logger::Init();
	const std::string platformString = std::format("{} ({}, {})", PLATFORM_NAME, BUILD_MODE, ARCH_NAME);
#ifdef CRASH_LOGGING
	InitCrashHandler(platformString);
#endif
	InitShutdownHandling();
	// Parse CLI Args
	// Args args{ { argc, argv } };
	MathHelper::InitSinTable();

	// Everything is ready to go!	
	Logger::Info << "Running on " << platformString << "\n";
#ifdef BUILD_SERVER
	RunServer();
#endif
#ifdef BUILD_CLIENT
	RunClient();
#endif

	// Prep everything for shutdown
	Logger::Close();
	Platform::Shutdown();
	return 0;
}