/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "command_manager.h"
#include "command.h"
#include "command_registry.h"
#include "logger.h"
#include <string_view>

void CommandManager::Init(Server* _server) {
	server = _server;
	RegisterHelp(dispatcher);
	RegisterTeleport(dispatcher);
	RegisterTime(dispatcher);
	RegisterSeed(dispatcher);
	RegisterSpawn(dispatcher);
	RegisterGive(dispatcher);
	RegisterList(dispatcher);
	RegisterDimension(dispatcher);
	RegisterAbout(dispatcher);
	RegisterSummon(dispatcher);
	RegisterStats(dispatcher);
	RegisterFill(dispatcher);
	RegisterStop(dispatcher);
	RegisterOp(dispatcher);
	RegisterWhitelist(dispatcher);
	RegisterKick(dispatcher);
	RegisterBan(dispatcher);
	RegisterAddon(dispatcher);
	RegisterTick(dispatcher);
	RegisterWeather(dispatcher);
	RegisterExecute(dispatcher);
	RegisterChat(dispatcher);
	RegisterGamerule(dispatcher);
	RegisterHardware(dispatcher);
	RegisterKill(dispatcher);
	//RegisterCrash(dispatcher);
	Logger::Info << "Registered " << dispatcher.root().children.size() << " command(s)!" << "\n";
}

std::vector<strategos::Suggestion> CommandManager::Suggest(std::string_view input, bool canOperate,
		std::span<const std::string> players) const {
	return dispatcher.suggest(input, canOperate, players);
}

bool CommandManager::Parse(std::string_view line, CommandContext& ctx) {
	ctx.server = server;
	ctx.manager = this;
	ctx.dispatcher = &dispatcher;
	ctx.executionSucceeded = true;
	while (!line.empty() && line.front() == ' ')
		line.remove_prefix(1);
	if (!line.empty() && line.front() == '/')
		line.remove_prefix(1);
	while (!line.empty() && line.front() == ' ')
		line.remove_prefix(1);
	try {
		if (line.empty()) {
			SendFeedback(ctx, "§c" ERROR_REASON_NO_CMD);
			return false;
		}
		auto result = dispatcher.execute(line, &ctx, [](void* userData) { return IsOperator(CmdCtx(userData)); });
		if (!result) {
			const auto& err = result.error();
			std::string message = err.message;
			if (err.error == strategos::ParseError::NoPermission)
				message = ERROR_OPERATOR;
			else if (err.error == strategos::ParseError::UnknownCommand && err.position == 0)
				message = ERROR_REASON_NO_EXIST;
			SendFeedback(ctx, "§c" + message);
			return false;
		}
		if (!result->empty()) {
			SendFeedback(ctx, "§c" + *result);
			return false;
		}
		return ctx.executionSucceeded;
	} catch (const std::exception& e) {
		Logger::Info << e.what() << " on /" << std::string(line) << "\n";
		SendFeedback(ctx, "§c" ERROR_REASON_ERROR);
	} catch (...) {
		Logger::Error << "Non-standard exception on /" << std::string(line) << "\n";
		SendFeedback(ctx, "§c" ERROR_REASON_ERROR);
	}
	return false;
}
