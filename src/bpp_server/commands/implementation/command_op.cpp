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


std::string OpPlayer(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto targetName = ResolveCmdTargetName(_cmd, ctx);
	if (!targetName) return ERROR_PLAYER_NAME_REQUIRED;
	std::string name = *targetName;

	if (!IsValidUsername(name))
		return "Invalid username!";
	auto it = FindUsername(ctx.server->operatorUsernames, name);
	if (it != ctx.server->operatorUsernames.end())
		return "User is already an operator!";
	ctx.server->operatorUsernames.push_back(NormalizeUsername(name));
	if (!ctx.server->SaveOperators())
		return "Failed to save ops!";
	SendFeedback(ctx, std::format("{} has been opped!", name));
	return "";
}

std::string DeopPlayer(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto targetName = ResolveCmdTargetName(_cmd, ctx);
	if (!targetName) return ERROR_PLAYER_NAME_REQUIRED;
	std::string name = *targetName;

	auto it = FindUsername(ctx.server->operatorUsernames, name);
	if (it == ctx.server->operatorUsernames.end())
		return "User is not an operator!";
	ctx.server->operatorUsernames.erase(it);
	if (!ctx.server->SaveOperators())
		return "Failed to save ops!";
	SendFeedback(ctx, std::format("{} has been deopped!", name));
	return "";
}

} // namespace

void RegisterOp(strategos::BrigadierContext& _dispatcher) {
	_dispatcher.add_command(strategos::Node::literal("op")
	                            .describe("Grants a player's operator privileges")
	                            .op()
	                            .executes(OpPlayer)
	                            .then(strategos::Node::string("username")
	                                      .suggests(strategos::SuggestionSource::Players)
	                                      .executes(OpPlayer)));
	_dispatcher.add_command(strategos::Node::literal("deop")
	                            .describe("Revokes a player's operator privileges")
	                            .op()
	                            .executes(DeopPlayer)
	                            .then(strategos::Node::string("username")
	                                      .suggests(strategos::SuggestionSource::Players)
	                                      .executes(DeopPlayer)));
}
