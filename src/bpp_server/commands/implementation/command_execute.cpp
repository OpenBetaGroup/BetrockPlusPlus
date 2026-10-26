/*
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../command.h"
#include "../command_manager.h"
#include "../command_registry.h"
#include "server.h"

namespace {

std::string ExecuteAsTarget(const strategos::CmdNode& command, void* userData) {
	auto& ctx = CmdCtx(userData);
	if (ctx.executionDepth >= 16) return "Maximum execute depth exceeded!";

	auto targetArg = command.get_arg<std::string>("target");
	auto nestedCommand = command.get_arg<std::string>("command");
	if (!targetArg || !nestedCommand) return ERROR_REASON_PARAMETERS;

	auto targets = ResolveCmdTargets(*targetArg, ctx);
	if (!targets) return targets.error();
	for (const CommandTarget& target : *targets) {
		Entity* entity = target.entity;
		if (!entity || entity->isDead) continue;
		CommandContext nested = ctx;
		nested.executionEntity = entity;
		nested.session = target.session;
		nested.world = ctx.server->GetWorldForDimension(entity->dim);
		nested.executionDepth = ctx.executionDepth + 1;
		if (!ctx.manager->Parse(*nestedCommand, nested) || !nested.executionSucceeded)
			ctx.executionSucceeded = false;
	}
	return "";
}

} // namespace

void RegisterExecute(strategos::BrigadierContext& dispatcher) {
	dispatcher.add_command(strategos::Node::literal("execute")
	                           .describe("Runs a command as another target")
	                           .op()
	                           .then(strategos::Node::string("target")
	                                     .suggests(strategos::SuggestionSource::Players)
	                                     .then(strategos::Node::greedy_string("command")
	                                               .suggests(strategos::SuggestionSource::NestedCommand)
	                                               .executes(ExecuteAsTarget))));
}
