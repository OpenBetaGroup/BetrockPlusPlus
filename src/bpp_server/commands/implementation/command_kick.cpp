/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../command.h"
#include "../command_manager.h"
#include "../command_registry.h"
#include "server.h"
#include "username.h"
#include <algorithm>
#include <format>

namespace {

std::string KickPlayer(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto targetName = ResolveCmdTargetName(_cmd, ctx);
	if (!targetName) return ERROR_PLAYER_NAME_REQUIRED;
	std::string name = *targetName;
	if (!name.empty() && name.front() == '@') {
		auto targets = ResolveCmdTargets(name, ctx);
		if (!targets) return targets.error();
		for (const CommandTarget& resolved : *targets) {
			if (!resolved.session) {
				SendFeedback(ctx, "Kick requires a player target; skipped non-player entity.");
				continue;
			}

			auto* target = resolved.session;
			ctx.server->DisconnectPlayer("Kicked by an operator", *target);
			if (target != ctx.sender)
				SendFeedback(ctx, std::format("{} has been kicked!", target->username));
		}
		return "";
	}
	if (!IsValidUsername(name)) return "Invalid username!";
	auto target = ctx.server->GetSessionByUsername(name);
	if (!target) return std::format("{} is not connected!", name);
	ctx.server->DisconnectPlayer("Kicked by an operator", *target);
	if (target.get() != ctx.sender)
		SendFeedback(ctx, std::format("{} has been kicked!", name));
	return "";
}

} // namespace

void RegisterKick(strategos::BrigadierContext& _dispatcher) {
	_dispatcher.add_command(strategos::Node::literal("kick")
	                            .describe("Forcefully disconnects the named player")
	                            .op()
	                            .executes(KickPlayer)
	                            .then(strategos::Node::string("username")
	                                      .suggests(strategos::SuggestionSource::Players)
	                                      .executes(KickPlayer)));
}
