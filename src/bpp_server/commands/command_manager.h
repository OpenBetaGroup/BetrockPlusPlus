/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#pragma once
#include "../player_conn/player_session.h"
#include "strategos.h"
#include "world.h"
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
struct Entity;
class Server;
class CommandManager;

struct CommandContext {
	PlayerSession* session = nullptr;
	Entity* executionEntity = nullptr;
	WorldManager* world = nullptr;
	Server* server = nullptr;
	std::function<void(PlayerSession&)> transferDimension;
	const strategos::BrigadierContext* dispatcher = nullptr;
	PlayerSession* sender = nullptr;
	CommandManager* manager = nullptr;
	uint8_t executionDepth = 0;
	bool executionSucceeded = true;
};

inline CommandContext& CmdCtx(void* _userData) {
	return *static_cast<CommandContext*>(_userData);
}

class CommandManager {
public:
	void Init(Server* _server);
	std::vector<strategos::Suggestion> Suggest(std::string_view input, bool canOperate,
	    std::span<const std::string> players) const;
	bool Parse(std::string_view _cmdString, CommandContext& _ctx);
	const strategos::BrigadierContext& Dispatcher() const noexcept {
		return dispatcher;
	}

private:
	Server* server = nullptr;
	strategos::BrigadierContext dispatcher;
};
