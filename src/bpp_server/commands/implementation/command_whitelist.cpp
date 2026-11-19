/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../command.h"
#include "../command_manager.h"
#include "../command_registry.h"
#include "commands/strategos.h"
#include "server.h"
#include "username.h"
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <vector>

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

void WhitelistAdd(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto name = _cmd.get_arg<std::string>("username");
	if (!name) {
		SendErrorFeedback(ctx, ERROR_REASON_TOO_FEW_PARAMETERS);
		return;
	}
	if (!IsValidUsername(*name)) {
		SendErrorFeedback(ctx, "Invalid username!");
		return;
	}
	WhitelistSession list(*ctx.server);
	auto it = FindUsername(ctx.server->whitelistedUsernames, *name);
	if (it != ctx.server->whitelistedUsernames.end()) {
		SendErrorFeedback(ctx, "User is already on whitelist!");
		return;
	}
	ctx.server->whitelistedUsernames.push_back(NormalizeUsername(*name));
	if (!ctx.server->SaveWhitelist()) {
		SendErrorFeedback(ctx, "Failed to save whitelist!");
		return;
	}
	SendFeedback(ctx, std::format("{} has been added to the whitelist!", *name));
}

void WhitelistRemove(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto name = _cmd.get_arg<std::string>("username");
	if (!name) {
		SendErrorFeedback(ctx, ERROR_REASON_TOO_FEW_PARAMETERS);
		return;
	}
	WhitelistSession list(*ctx.server);
	auto it = FindUsername(ctx.server->whitelistedUsernames, *name);
	if (it == ctx.server->whitelistedUsernames.end()) {
		SendErrorFeedback(ctx, "User is not on the whitelist!");
		return;
	}
	ctx.server->whitelistedUsernames.erase(it);
	if (!ctx.server->SaveWhitelist()) {
		SendErrorFeedback(ctx, "Failed to save whitelist!");
		return;
	}
	SendFeedback(ctx, std::format("{} has been removed from the whitelist!", *name));
}

void WhitelistOn(const strategos::CmdNode&, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	ctx.server->SetWhitelistEnabled(true);
	SendFeedback(ctx, "Whitelist has been enabled!");
}

void WhitelistOff(const strategos::CmdNode&, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	ctx.server->SetWhitelistEnabled(false);
	SendFeedback(ctx, "Whitelist has been disabled!");
}

void WhitelistList(const strategos::CmdNode&, void* _userData) {
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
}

void WhitelistReload(const strategos::CmdNode&, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	if (!ctx.server->useWhitelist) {
		SendErrorFeedback(ctx, "Whitelist is not enabled!");
		return;
	}
	ctx.server->ReloadWhitelist();
	SendFeedback(ctx, "Reloaded whitelist!");
}

} // namespace

void RegisterWhitelist(strategos::BrigadierContext& _dispatcher) {
	_dispatcher.add_command(
	    strategos::Node::literal("whitelist")
	        .describe("Adjust the servers whitelist/allowlist")
	        .op()
	        .then(strategos::Node::literal("add").then(
	            strategos::Node::string("username").suggests(strategos::SuggestionSource::Players).executes(WhitelistAdd)))
	        .then(strategos::Node::literal("remove").then(strategos::Node::string("username")
	                                                          .suggests(strategos::SuggestionSource::Players)
	                                                          .executes(WhitelistRemove)))
	        .then(strategos::Node::literal("list").executes(WhitelistList))
	        .then(strategos::Node::literal("reload").executes(WhitelistReload))
	        .then(strategos::Node::literal("on").executes(WhitelistOn))
	        .then(strategos::Node::literal("off").executes(WhitelistOff)));
}
