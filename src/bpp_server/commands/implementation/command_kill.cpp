/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "../command.h"
#include "../command_manager.h"
#include "../command_registry.h"
#include "server.h"
#include "username.h"
#include <algorithm>
#include <format>

namespace {

// Kills the session that ran the command
std::string KillSelf(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto playerEntity = static_cast<PlayerEntity*>(ctx.sender->entity.get());
	playerEntity->AttackEntityFrom(nullptr, playerEntity->health, DamageType::Command);
	return "";
}

// Kill others
std::string KillOther(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto targets = _cmd.get_arg<std::string>("player");
	if (!targets)
		return ERROR_REASON_PARAMETERS;
	if (!IsOperator(ctx))
		return ERROR_OPERATOR;
	if (!targets->empty()) {
		if (targets->front() == '@') {
			auto collected = ResolveCmdTargets(*targets, ctx);
			if (!collected)
				return collected.error();

			size_t killed = 0;
			for (const CommandTarget& resolved : *collected) {
				killed++;
				auto target = resolved.entity;
				auto isMobile = dynamic_cast<MobileEntity*>(target);
				if (isMobile)
					isMobile->AttackEntityFrom(nullptr, isMobile->health, DamageType::Command);
				else {
					target->isDead = true;
				}
			}
			SendFeedback(ctx, "§eKilled (" + std::to_string(killed) + ") entities!");
			return "";
		} else {
			auto targetSession = ctx.server->GetSessionByUsername(*targets);
			if (!targetSession)
				return "Invalid player target!";
			auto playerEntity = static_cast<PlayerEntity*>(targetSession->entity.get());
			playerEntity->AttackEntityFrom(nullptr, playerEntity->health, DamageType::Command);
			return "";
		}
	}
	KillSelf(_cmd, _userData);
	return "";
}

} // namespace

void RegisterKill(strategos::BrigadierContext& _dispatcher) {
	_dispatcher.add_command(
	    strategos::Node::literal("kill")
	        .describe("Kills a player or entity")
	        .op()
	        .executes(KillSelf)
	        .then(strategos::Node::string("player").suggests(strategos::SuggestionSource::Players).executes(KillOther)));
}
