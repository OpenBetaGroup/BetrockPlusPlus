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
#include <optional>
#include <string>
#include <vector>

namespace {

struct CommandTreeCollector {
	struct Usage {
		std::string line;
		bool requiresOperator;
	};

	std::vector<Usage> usages;
	bool allUsagesAreOp = true;
	std::vector<const strategos::Node*> arguments;

	void Collect(const strategos::Node& _node, const std::string& _prefix = "") {
		std::string token = _node.type == strategos::NodeType::Literal ? _node.name : "<" + _node.name + ">";
		std::string accum = _prefix.empty() ? std::move(token) : _prefix + " " + token;

		if (_node.action) {
			usages.push_back(Usage{ .line = accum, .requiresOperator = _node.requiresOperator });
			if (!_node.requiresOperator)
				allUsagesAreOp = false;
		}

		if (_node.type != strategos::NodeType::Literal && !_node.description.empty() &&
		    std::ranges::none_of(arguments, [&](const auto* _arg) { return _arg->name == _node.name; }))
			arguments.push_back(&_node);

		for (const auto& child : _node.children)
			Collect(child, std::move(accum));
	}
};

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

		CommandTreeCollector collector;
		collector.Collect(cmd);
		for (const auto& usage : collector.usages) {
			std::string line = "/" + usage.line;
			if (usage.requiresOperator && !collector.allUsagesAreOp)
				line += " (Requires operator)";
			SendFeedback(ctx, "§7" + line);
		}
		for (const auto& arg : collector.arguments) {
			std::string line = std::format("§7<{}> - {}", arg->name, arg->description);
			SendFeedback(ctx, line);
		}
		if (collector.allUsagesAreOp)
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
