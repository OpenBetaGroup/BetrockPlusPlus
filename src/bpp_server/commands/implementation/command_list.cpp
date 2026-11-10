/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../../server.h"
#include "../command.h"
#include "../command_manager.h"
#include "../command_registry.h"
#include <format>

namespace {

std::string ListPlayers(const strategos::CmdNode&, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	const auto& players = ctx.server->GetPlayers();
	SendFeedback(ctx, std::format("§7-- {} Player(s) --", players.size()));
	std::string line = "§7";
	for (size_t i = 0; i < players.size(); i++) {
		auto& p = players[i];
		if (p->connState != ConnectionState::Playing)
			continue;
		if (line.size() + p->username.size() > 64) {
			SendFeedback(ctx, line);
			line = "§7";
		}
		line += p->username + ((i < (players.size() - 1)) ? ", " : "");
	}
	SendFeedback(ctx, line);
	return "";
}

} // namespace

void RegisterList(strategos::BrigadierContext& _dispatcher) {
	_dispatcher.add_command(
	    strategos::Node::literal("list").describe("List all currently online players").executes(ListPlayers));
}
