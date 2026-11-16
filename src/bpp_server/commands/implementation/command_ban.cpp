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
#include <cctype>
#include <format>
#include <sstream>

namespace {

bool IsValidIpAddress(const std::string& _ip) {
	std::stringstream stream(_ip);
	std::string segment;
	int segments = 0;
	while (std::getline(stream, segment, '.')) {
		segments++;
		if (segments > 4 || segment.empty() || segment.size() > 3)
			return false;
		for (unsigned char c : segment) {
			if (!std::isdigit(c))
				return false;
		}
		if (std::stoi(segment) > 255)
			return false;
	}
	return segments == 4;
}

void BanPlayer(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto targetName = ResolveCmdTargetName(_cmd, ctx);
	if (!targetName) {
		SendErrorFeedback(ctx, ERROR_PLAYER_NAME_REQUIRED);
		return;
	}
	if (!targetName->empty() && targetName->front() == '@') {
		auto targets = ResolveCmdPlayerTargets(*targetName, ctx, "Ban");
		if (!targets) {
			SendErrorFeedback(ctx, targets.error());
			return;
		}

		for (const CommandTarget& target : *targets) {
			auto& name = target.session->username;
			auto it = std::find(ctx.server->bannedUsernames.begin(), ctx.server->bannedUsernames.end(), name);
			if (it != ctx.server->bannedUsernames.end()) {
				SendFeedback(ctx, name + " is already banned!");
				continue;
			}

			ctx.server->bannedUsernames.push_back(name);
			if (!ctx.server->SaveBannedPlayers()) {
				SendErrorFeedback(ctx, "Failed to save banned players!");
				return;
			}
			ctx.server->DisconnectPlayer("You are banned from this server!", *target.session);
			SendFeedback(ctx, std::format("{} has been banned!", name));
		}
		return;
	}
	std::string name = *targetName;
	if (!IsValidUsername(name)) {
		SendErrorFeedback(ctx, "Invalid username!");
		return;
	}
	auto it = FindUsername(ctx.server->bannedUsernames, name);
	if (it != ctx.server->bannedUsernames.end()) {
		SendErrorFeedback(ctx, "User is already banned!");
		return;
	}
	ctx.server->bannedUsernames.push_back(NormalizeUsername(name));
	if (!ctx.server->SaveBannedPlayers()) {
		SendErrorFeedback(ctx, "Failed to save banned players!");
		return;
	}
	// Boot them immediately if they're currently online.
	if (auto target = ctx.server->GetSessionByUsername(name))
		ctx.server->DisconnectPlayer("You are banned from this server!", *target);
	SendFeedback(ctx, std::format("{} has been banned!", name));
}

void PardonPlayer(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto targetName = ResolveCmdTargetName(_cmd, ctx);
	if (!targetName) {
		SendErrorFeedback(ctx, ERROR_PLAYER_NAME_REQUIRED);
		return;
	}
	if (!targetName->empty() && targetName->front() == '@') {
		auto targets = ResolveCmdPlayerTargets(*targetName, ctx, "Pardon");
		if (!targets) {
			SendErrorFeedback(ctx, targets.error());
			return;
		}

		for (const CommandTarget& target : *targets) {
			const auto& name = target.session->username;
			auto it = std::find(ctx.server->bannedUsernames.begin(), ctx.server->bannedUsernames.end(), name);
			if (it == ctx.server->bannedUsernames.end()) {
				SendFeedback(ctx, name + " is not banned!");
				continue;
			}

			ctx.server->bannedUsernames.erase(it);
			if (!ctx.server->SaveBannedPlayers()) {
				SendErrorFeedback(ctx, "Failed to save banned players!");
				return;
			}
			SendFeedback(ctx, std::format("{} has been pardoned!", name));
		}
		return;
	}
	std::string name = *targetName;
	auto it = FindUsername(ctx.server->bannedUsernames, name);
	if (it == ctx.server->bannedUsernames.end()) {
		SendErrorFeedback(ctx, "User is not banned!");
		return;
	}
	ctx.server->bannedUsernames.erase(it);
	if (!ctx.server->SaveBannedPlayers()) {
		SendErrorFeedback(ctx, "Failed to save banned players!");
		return;
	}
	SendFeedback(ctx, std::format("{} has been pardoned!", name));
}

void BanIp(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto arg = _cmd.get_arg<std::string>("target");
	if (!arg) {
		SendErrorFeedback(ctx, ERROR_REASON_TOO_FEW_PARAMETERS);
		return;
	}
	std::string ip = *arg;
	// Allow the target to be either a raw IP, or the username of a currently connected player.
	if (!IsValidIpAddress(ip)) {
		auto target = ctx.server->GetSessionByUsername(ip);
		if (!target || target->ipAddress.empty()) {
			SendErrorFeedback(ctx, "Not a valid IP, and no connected player by that name!");
			return;
		}
		ip = target->ipAddress;
	}
	auto it = std::find(ctx.server->bannedIps.begin(), ctx.server->bannedIps.end(), ip);
	if (it != ctx.server->bannedIps.end()) {
		SendErrorFeedback(ctx, "IP is already banned!");
		return;
	}
	ctx.server->bannedIps.push_back(ip);
	if (!ctx.server->SaveBannedIps()) {
		SendErrorFeedback(ctx, "Failed to save banned IPs!");
		return;
	}
	// Boot anyone currently connected from this IP.
	for (auto& player : ctx.server->GetPlayers()) {
		if (player->ipAddress == ip)
			ctx.server->DisconnectPlayer("Your IP address is banned from this server!", *player);
	}
	SendFeedback(ctx, std::format("{} has been IP banned!", ip));
}

/*
std::string PardonIp(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto arg = _cmd.get_arg<std::string>("ip");
	if (!arg)
		return ERROR_REASON_TOO_FEW_PARAMETERS;
	if (!IsValidIpAddress(*arg))
		return "Invalid IP address!";
	auto it = std::find(ctx.server->bannedIps.begin(), ctx.server->bannedIps.end(), *arg);
	if (it == ctx.server->bannedIps.end())
		return "IP is not banned!";
	ctx.server->bannedIps.erase(it);
	if (!ctx.server->SaveBannedIps())
		return "Failed to save banned IPs!";
	SendChat(*ctx.session, std::format("{} has been un-banned!", *arg));
	return "";
}
*/

void BanListPlayers(const strategos::CmdNode&, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	const auto& names = ctx.server->bannedUsernames;
	SendChunkedList(ctx, std::format("§7-- {} Banned Player(s) --", names.size()), names);
}

void BanListIps(const strategos::CmdNode&, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	const auto& ips = ctx.server->bannedIps;
	SendChunkedList(ctx, std::format("§7-- {} Banned IP(s) --", ips.size()), ips);
}

} // namespace

void RegisterBan(strategos::BrigadierContext& _dispatcher) {
	_dispatcher.add_command(
	    strategos::Node::literal("ban")
	        .describe("Bans a player from the server")
	        .op()
	        .executes(BanPlayer)
	        .then(
	            strategos::Node::string("username").suggests(strategos::SuggestionSource::Players).executes(BanPlayer)));
	_dispatcher.add_command(strategos::Node::literal("pardon")
	                            .describe("Removes a player from the ban list")
	                            .op()
	                            .executes(PardonPlayer)
	                            .then(strategos::Node::string("username")
	                                      .suggests(strategos::SuggestionSource::Players)
	                                      .executes(PardonPlayer)));
	_dispatcher.add_command(
	    strategos::Node::literal("ban-ip")
	        .describe("Bans an IP address, or a connected player's IP, from the server")
	        .op()
	        .then(strategos::Node::string("target").suggests(strategos::SuggestionSource::Players).executes(BanIp))); /*
	_dispatcher.add_command(strategos::Node::literal("pardon-ip")
	                            .describe("Removes an IP address from the ban list")
	                            .op()
	                            .then(strategos::Node::string("ip").executes(PardonIp)));
	*/
	_dispatcher.add_command(strategos::Node::literal("banlist")
	                            .describe("Lists banned players or IP addresses")
	                            .op()
	                            .executes(BanListPlayers)
	                            .then(strategos::Node::literal("players").executes(BanListPlayers))
	                            .then(strategos::Node::literal("ips").executes(BanListIps)));
}
