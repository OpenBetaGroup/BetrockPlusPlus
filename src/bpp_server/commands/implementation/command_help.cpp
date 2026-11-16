/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../command.h"
#include "../command_manager.h"
#include "../command_registry.h"

namespace {

void CollectUsages(const strategos::Node& _node, const std::string& _prefix, std::vector<std::string>& _usages) {
	std::string token;
	if (_node.type == strategos::NodeType::Literal)
		token = _node.name;
	else if (_node.type != strategos::NodeType::Root)
		token = "<" + _node.name + ">";
	std::string here = _prefix.empty() ? token : _prefix + " " + token;
	if (_node.action)
		_usages.push_back(here);
	for (const auto& child : _node.children)
		CollectUsages(child, here, _usages);
}

bool CanSeeCommand(const strategos::Node& _cmd, CommandContext& _ctx) {
	return !_cmd.requiresOperator || IsOperator(_ctx);
}

void ListCommands(const strategos::CmdNode&, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	const auto& commands = ctx.dispatcher->root().children;

	SendFeedback(ctx, "§7-- All commands --");
	std::string line = "§7";
	bool first = true;
	for (const auto& cmd : commands) {
		if (!CanSeeCommand(cmd, ctx))
			continue;
		if (!first)
			line += ", ";
		first = false;
		if (line.size() + cmd.name.size() > MAX_CHAT_LINE_SIZE) {
			SendFeedback(ctx, line);
			line = "§7" + cmd.name;
		} else {
			line += cmd.name;
		}
	}
	if (line != "§7")
		SendFeedback(ctx, line);
}

void HelpCommand(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto name = _cmd.get_arg<std::string>("command");
	if (!name) {
		SendErrorFeedback(ctx, "Command not found!");
		return;
	}

	for (const auto& cmd : ctx.dispatcher->root().children) {
		if (cmd.name != *name)
			continue;
		if (!CanSeeCommand(cmd, ctx)) {
			SendErrorFeedback(ctx, ERROR_PERMISSIONS);
			return;
		}
		SendFeedback(ctx, "§7" + cmd.name + ": " + cmd.description);
		std::vector<std::string> usages;
		CollectUsages(cmd, "", usages);
		for (const auto& usage : usages) {
			std::string line = "/" + usage;
			if (line.size() > cmd.name.size() + 1) // more than "/name"
				SendFeedback(ctx, "§7" + line);
		}
		if (cmd.requiresOperator)
			SendFeedback(ctx, "§7(Requires operator)");
		return;
	}
	SendErrorFeedback(ctx, "Command not found!");
}

} // namespace

void RegisterHelp(strategos::BrigadierContext& _dispatcher) {
	_dispatcher.add_command(strategos::Node::literal("help")
	                            .describe("Lists commands or helps with command")
	                            .executes(ListCommands)
	                            .then(strategos::Node::string("command")
	                                      .suggests(strategos::SuggestionSource::Commands)
	                                      .executes(HelpCommand)));
}
