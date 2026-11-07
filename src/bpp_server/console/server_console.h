/*
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */
#pragma once

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "platform/platform_caps.h"
#ifndef BPP_HEADLESS_CONSOLE
#include <replxx.hxx>
#endif

class CommandManager;

class ServerConsole {
public:
	ServerConsole();
	~ServerConsole();
	ServerConsole(const ServerConsole&) = delete;
	ServerConsole& operator=(const ServerConsole&) = delete;

	void Start(const CommandManager& _commands);
	void Stop();
	void DrainCommands(std::vector<std::string>& _commands);
	void UpdatePlayerNames(const std::vector<std::string>& _names);

private:
	void ReaderLoop();
	void QueueLine(std::string _line);
#ifndef BPP_HEADLESS_CONSOLE
	replxx::Replxx::completions_t Complete(std::string_view _input, int& _contextLen) const;
#endif

	const CommandManager* commands = nullptr;
	mutable std::mutex namesMutex;
	std::vector<std::string> playerNames;
	std::mutex queueMutex;
	std::deque<std::string> commandQueue;
	std::thread reader;
	std::atomic<bool> readerDone{ true };
	std::atomic<bool> stopping{ false };
	bool interactive = false;
	struct Impl;
	std::unique_ptr<Impl> impl;
};
