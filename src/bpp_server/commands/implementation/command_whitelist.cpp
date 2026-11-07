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

// Load the whitelist for this command; unload again if it stays disabled.
struct WhitelistSession {
	Server& server;
	const bool unloadOnExit;

	explicit WhitelistSession(Server& _server) : server(_server), unloadOnExit(!_server.useWhitelist) {
		server.LoadWhitelist();
	}

	~WhitelistSession() {
		if (unloadOnExit)
			server.UnloadWhitelist();
	}

	WhitelistSession(const WhitelistSession&) = delete;
	WhitelistSession& operator=(const WhitelistSession&) = delete;
};

std::string WhitelistAdd(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto name = _cmd.get_arg<std::string>("username");
	if (!name)
		return ERROR_REASON_TOO_FEW_PARAMETERS;
	if (!IsValidUsername(*name))
		return "Invalid username!";
	WhitelistSession list(*ctx.server);
	auto it = FindUsername(ctx.server->whitelistedUsernames, *name);
	if (it != ctx.server->whitelistedUsernames.end())
		return "User is already on whitelist!";
	ctx.server->whitelistedUsernames.push_back(NormalizeUsername(*name));
	if (!ctx.server->SaveWhitelist())
		return "Failed to save whitelist!";
	SendFeedback(ctx, std::format("{} has been added to the whitelist!", *name));
	return "";
}

std::string WhitelistRemove(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto name = _cmd.get_arg<std::string>("username");
	if (!name)
		return ERROR_REASON_TOO_FEW_PARAMETERS;
	WhitelistSession list(*ctx.server);
	auto it = FindUsername(ctx.server->whitelistedUsernames, *name);
	if (it == ctx.server->whitelistedUsernames.end())
		return "User is not on the whitelist!";
	ctx.server->whitelistedUsernames.erase(it);
	if (!ctx.server->SaveWhitelist())
		return "Failed to save whitelist!";
	SendFeedback(ctx, std::format("{} has been removed from the whitelist!", *name));
	return "";
}

std::string WhitelistOn(const strategos::CmdNode&, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	ctx.server->SetWhitelistEnabled(true);
	SendFeedback(ctx, "Whitelist has been enabled!");
	return "";
}

std::string WhitelistOff(const strategos::CmdNode&, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	ctx.server->SetWhitelistEnabled(false);
	SendFeedback(ctx, "Whitelist has been disabled!");
	return "";
}

std::string WhitelistList(const strategos::CmdNode&, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	WhitelistSession list(*ctx.server);
	const auto& names = ctx.server->whitelistedUsernames;
	SendFeedback(ctx, std::format("§7-- {} Whitelisted Player(s) --", names.size()));
	std::string line = "§7";
	for (size_t i = 0; i < names.size(); i++) {
		auto& p = names[i];
		const std::string suffix = (i < (names.size() - 1)) ? ", " : "";
		if (line.size() + p.size() + suffix.size() > 64 && line != "§7") {
			SendFeedback(ctx, line);
			line = "§7";
		}
		line += p + suffix;
	}
	if (line != "§7")
		SendFeedback(ctx, line);
	return "";
}

std::string WhitelistReload(const strategos::CmdNode&, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	if (!ctx.server->useWhitelist)
		return "Whitelist is not enabled!";
	ctx.server->ReloadWhitelist();
	SendFeedback(ctx, "Reloaded whitelist!");
	return "";
}

} // namespace

void RegisterWhitelist(strategos::BrigadierContext& _dispatcher) {
	_dispatcher.add_command(
	    strategos::Node::literal("whitelist")
	        .describe("Adjust the servers whitelist/allowlist")
	        .op()
	        .then(strategos::Node::literal("add")
	                  .then(strategos::Node::string("username")
	                            .suggests(strategos::SuggestionSource::Players)
	                            .executes(WhitelistAdd)))
	        .then(strategos::Node::literal("remove")
	                  .then(strategos::Node::string("username")
	                            .suggests(strategos::SuggestionSource::Players)
	                            .executes(WhitelistRemove)))	        .then(strategos::Node::literal("list").executes(WhitelistList))
	        .then(strategos::Node::literal("reload").executes(WhitelistReload))
	        .then(strategos::Node::literal("on").executes(WhitelistOn))
	        .then(strategos::Node::literal("off").executes(WhitelistOff)));
}
