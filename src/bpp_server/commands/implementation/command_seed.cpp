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
#include <string>

namespace {

void ShowSeed(const strategos::CmdNode&, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	SendFeedback(ctx, "§e" + std::to_string(ctx.world->seed));
}

} // namespace

void RegisterSeed(strategos::BrigadierContext& _dispatcher) {
	_dispatcher.add_command(strategos::Node::literal("seed").describe("Get the world seed").op().executes(ShowSeed));
}
