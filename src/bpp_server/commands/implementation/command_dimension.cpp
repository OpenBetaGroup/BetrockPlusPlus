/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../command.h"
#include "../command_manager.h"
#include "../command_registry.h"
#include "server.h"

namespace {

std::string SwapDimension(const strategos::CmdNode&, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	if (!ctx.session || !ctx.session->entity)
		return ERROR_PLAYER_REQUIRED;
	SendFeedback(ctx, ctx.session->dimension == Dimension::Overworld ? "§7Transferring to the Nether..."
	                                                                 : "§7Transferring to the Overworld...");

	Dimension newDim = ctx.session->dimension == Dimension::Nether ? Dimension::Overworld : Dimension::Nether;
	ctx.server->SendPlayerToDimension(newDim, *ctx.session);
	return "";
}

} // namespace

void RegisterDimension(strategos::BrigadierContext& _dispatcher) {
	_dispatcher.add_command(
	    strategos::Node::literal("dim").describe("Swap to the other dimension").op().executes(SwapDimension));
}
