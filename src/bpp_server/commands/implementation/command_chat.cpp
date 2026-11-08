/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../command.h"
#include "../command_manager.h"
#include "../command_registry.h"
#include "server.h"
#include "username.h"

namespace {
// TODO: Still needs to have chat formatting + who sent it!!!!!
std::string SayMessage(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto message = _cmd.get_arg<std::string>("message");
	if (!message)
		return "Empty message!";
	ctx.server->SendGlobalChatMessage(*message);
	return "";
}

// TODO: Still needs to have chat formatting + who sent it!!!!!
std::string TellMessage(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto targetName = ResolveCmdTargetName(_cmd, ctx);
	if (!targetName)
		return ERROR_PLAYER_NAME_REQUIRED;
	std::string name = *targetName;
	if (!IsValidUsername(name))
		return "Invalid username!";
	auto session = ctx.server->GetSessionByUsername(name);
	if (!session)
		return "Target not found!";

	auto message = _cmd.get_arg<std::string>("message");
	if (!message)
		return "Empty message!";
	SendFeedback(ctx, *message);
	SendCommandFeedback(session.get(), *message);
	return "";
}

} // namespace

void RegisterChat(strategos::BrigadierContext& _dispatcher) {
	_dispatcher.add_command(strategos::Node::literal("say")
	                            .describe("Sends a message in the chat to other players")
	                            .then(strategos::Node::greedy_string("message").executes(SayMessage)));
	_dispatcher.add_command(strategos::Node::literal("tell")
	                            .describe("Sends a private message to one player")
	                            // TODO: Should accept a list of players, maybe?
	                            .then(strategos::Node::string("username")
	                                      .then(strategos::Node::greedy_string("message").executes(TellMessage))));
}
