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
#include "logstream.h"
#include "strategos.h"
#include <exception>
#include <optional>
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

std::vector<strategos::Suggestion> CommandManager::Suggest(std::string_view _input, bool _canOperate,
                                                           std::span<const std::string> _players) const {
	return dispatcher.suggest(_input, _canOperate, _players);
}

bool CommandManager::Parse(std::string_view _line, CommandContext& _ctx) {
	_ctx.server = server;
	_ctx.manager = this;
	_ctx.dispatcher = &dispatcher;
	_ctx.executionSucceeded = true;
	while (!_line.empty() && _line.front() == ' ')
		_line.remove_prefix(1);
	if (!_line.empty() && _line.front() == '/')
		_line.remove_prefix(1);
	while (!_line.empty() && _line.front() == ' ')
		_line.remove_prefix(1);
	try {
		if (_line.empty()) {
			SendErrorFeedback(_ctx, "Parse Error: " ERROR_REASON_NO_CMD);
			return false;
		}
		auto error = dispatcher.execute(_line, &_ctx, [](void* _userData) { return IsOperator(CmdCtx(_userData)); });
		if (error) {
			std::string message = error->message;
			if (error->type == strategos::ParseErrorType::NoPermission)
				message = ERROR_OPERATOR;
			else if (error->type == strategos::ParseErrorType::UnknownCommand && error->position == 0)
				message = ERROR_REASON_NO_EXIST;
			if (!message.empty())
				SendErrorFeedback(_ctx, "Parse Error: " + message);
			return false;
		}
		return _ctx.executionSucceeded;
	} catch (const std::exception& e) {
		Logger::Info << e.what() << " on /" << std::string(_line) << "\n";
		SendErrorFeedback(_ctx, "Parse Exception: " ERROR_REASON_ERROR);
	} catch (...) {
		Logger::Error << "Non-standard exception on /" << std::string(_line) << "\n";
		SendErrorFeedback(_ctx, "Parse Exception: " ERROR_REASON_ERROR);
	}
	return false;
}
