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

void KickPlayer(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto targetName = ResolveCmdTargetName(_cmd, ctx);
	if (!targetName) {
		SendErrorFeedback(ctx, ERROR_PLAYER_NAME_REQUIRED);
		return;
	}
	std::string name = *targetName;
	if (!name.empty() && name.front() == '@') {
		auto targets = ResolveCmdPlayerTargets(name, ctx, "Kick");
		if (!targets) {
			SendErrorFeedback(ctx, targets.error());
			return;
		}
		for (const CommandTarget& resolved : *targets) {
			auto* target = resolved.session;
			ctx.server->DisconnectPlayer("Kicked by an operator", *target);
			if (target != ctx.sender)
				SendFeedback(ctx, std::format("{} has been kicked!", target->username));
		}
		return;
	}
	if (!IsValidUsername(name)) {
		SendErrorFeedback(ctx, "Invalid username!");
		return;
	}
	auto target = ctx.server->GetSessionByUsername(name);
	if (!target) {
		SendErrorFeedback(ctx, std::format("{} is not connected!", name));
		return;
	}
	ctx.server->DisconnectPlayer("Kicked by an operator", *target);
	if (target.get() != ctx.sender)
		SendFeedback(ctx, std::format("{} has been kicked!", name));
}

} // namespace

void RegisterKick(strategos::BrigadierContext& _dispatcher) {
	_dispatcher.add_command(
	    strategos::Node::literal("kick")
	        .describe("Forcefully disconnects the named player")
	        .op()
	        .executes(KickPlayer)
	        .then(
	            strategos::Node::string("username").suggests(strategos::SuggestionSource::Players).executes(KickPlayer)));
}
