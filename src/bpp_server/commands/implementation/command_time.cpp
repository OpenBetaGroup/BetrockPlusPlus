/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../command.h"
#include "../command_manager.h"
#include "../command_registry.h"
#include "commands/strategos.h"
#include "world.h"
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>

namespace {

void GetTime(const strategos::CmdNode&, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	SendFeedback(ctx, "§eCurrent Time is " + std::to_string(ctx.world->elapsedTicks) + " (Day " +
	                      std::to_string(int64_t(std::floor(ctx.world->elapsedTicks / DAY_LENGTH))) + ")");
}

void SetTime(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto ticks = _cmd.get_arg<std::string>("ticks");
	if (!ticks) {
		SendErrorFeedback(ctx, ERROR_REASON_PARAMETERS);
		return;
	}
	try {
		ctx.world->elapsedTicks = std::stol(*ticks);
	} catch (...) {
		SendErrorFeedback(ctx, ERROR_REASON_PARAMETERS);
		return;
	}
	SendFeedback(ctx, "§eSet time to " + std::to_string(ctx.world->elapsedTicks));
}

void AddTime(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto ticks = _cmd.get_arg<std::string>("ticks");
	if (!ticks) {
		SendErrorFeedback(ctx, ERROR_REASON_PARAMETERS);
		return;
	}
	try {
		ctx.world->elapsedTicks += std::stol(*ticks);
	} catch (...) {
		SendErrorFeedback(ctx, ERROR_REASON_PARAMETERS);
		return;
	}
	SendFeedback(ctx, "§eSet time to " + std::to_string(ctx.world->elapsedTicks));
}

} // namespace

void RegisterTime(strategos::BrigadierContext& _dispatcher) {
	_dispatcher.add_command(
	    strategos::Node::literal("time")
	        .describe("Gets or sets the current world time")
	        .op()
	        .executes(GetTime)
	        .then(strategos::Node::literal("set").then(strategos::Node::string("ticks").executes(SetTime)))
	        .then(strategos::Node::literal("add").then(strategos::Node::string("ticks").executes(AddTime))));
}
