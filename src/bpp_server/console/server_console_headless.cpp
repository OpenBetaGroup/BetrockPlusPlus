/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

// Stand-in for server_console.cpp on targets without a terminal or a line editor

#include "server.h"

struct ServerConsole::Impl {};

ServerConsole::ServerConsole() = default;

ServerConsole::~ServerConsole() = default;

void ServerConsole::Start(const CommandManager& _commands) {
	commands = &_commands;
}

void ServerConsole::Stop() {}

void ServerConsole::DrainCommands(std::vector<std::string>& _commands) {
	std::lock_guard lock(queueMutex);
	while (!commandQueue.empty()) {
		_commands.push_back(std::move(commandQueue.front()));
		commandQueue.pop_front();
	}
}

void ServerConsole::UpdatePlayerNames(const std::vector<std::string>& _names) {
	std::lock_guard lock(namesMutex);
	if (playerNames != _names)
		playerNames = _names;
}
