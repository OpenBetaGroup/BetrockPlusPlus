/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../../packet/packet_utils.h"
#include "../command.h"
#include "../command_manager.h"
#include "../command_registry.h"
#include "items.h"
#include "strings/labels.h"
#include <string>
#include "server.h"

namespace {

void GiveItem(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	if (!ctx.session) {
		SendErrorFeedback(ctx, ERROR_PLAYER_REQUIRED);
		return;
	}
	auto itemArg = _cmd.get_arg<std::string>("item");
	if (!itemArg) {
		SendErrorFeedback(ctx, "Missing item id!");
		return;
	}

	std::optional<int> count;
	if (auto amount = _cmd.get_arg<int>("amount"))
		count = *amount;

	ItemStack item;
	try {
		item = ParseItemStack(*itemArg, count);
	} catch (...) {
		SendErrorFeedback(ctx, ERROR_REASON_PARAMETERS);
		return;
	}

	if (!Items::IsValidId(item.id)) {
		SendErrorFeedback(ctx, std::to_string(item.id) + " is not a valid item id!");
		return;
	}

	SendFeedback(ctx, "§eGave " + IdToLabel(item.id) + " (" + std::to_string(item.id) + ":" + std::to_string(item.data) +
	                      ") x" + std::to_string(item.count) + " to " + ctx.session->username);

	if (ctx.session->inventory.PickupItem(item))
		PacketUtilities::SendInventory(*ctx.session, ctx.session->openWindowId, ctx.session->inventory);
}

void ClearOwnInventory(const strategos::CmdNode& /*_cmd*/, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	if (!ctx.session) {
		SendErrorFeedback(ctx, ERROR_PLAYER_REQUIRED);
		return;
	}
	const auto count = ctx.session->inventory.Clear();
	PacketUtilities::SendInventory(*ctx.session, 0, ctx.session->inventory);
	SendFeedback(ctx, std::format("§eRemoved {} Item(s) from player {}", count, ctx.session->username));
}

void ClearOtherInventory(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto playerName = _cmd.get_arg<std::string>("player");
	if (!playerName) {
		SendErrorFeedback(ctx, ERROR_REASON_PARAMETERS);
		return;
	}
	if (!IsOperator(ctx)) {
		SendErrorFeedback(ctx, ERROR_OPERATOR);
		return;
	}
	if (!playerName->empty() && playerName->front() == '@') {
		auto targets = ResolveCmdPlayerTargets(*playerName, ctx, "Clear");
		if (!targets) {
			SendErrorFeedback(ctx, targets.error());
			return;
		}

		for (const CommandTarget& resolved : *targets) {
			auto& target = *resolved.session;
			const auto count = target.inventory.Clear();
			PacketUtilities::SendInventory(target, 0, target.inventory);
			SendFeedback(ctx, std::format("§eRemoved {} Item(s) from player {}", count, target.username));
		}
		return;
	}
	auto target = ctx.server->GetSessionByUsername(*playerName);
	if (!target) {
		SendErrorFeedback(ctx, *playerName + " does not exist!");
		return;
	}
	const auto count = target->inventory.Clear();
	PacketUtilities::SendInventory(*target, 0, target->inventory);
	SendFeedback(ctx, std::format("§eRemoved {} Item(s) from player {}", count, *playerName));
}

} // namespace

void RegisterGive(strategos::BrigadierContext& _dispatcher) {
	_dispatcher.add_command(strategos::Node::literal("give")
	                            .describe("Give yourself a block or item")
	                            .op()
	                            .then(strategos::Node::string("item").executes(GiveItem).then(
	                                strategos::Node::integer("amount").executes(GiveItem))));
	_dispatcher.add_command(strategos::Node::literal("clear")
	                            .describe("Clears your or another player's inventory")
	                            .op()
	                            .executes(ClearOwnInventory)
	                            .then(strategos::Node::string("player")
	                                      .describe("Target inventory to clear")
	                                      .op()
	                                      .suggests(strategos::SuggestionSource::Players)
	                                      .executes(ClearOtherInventory)));
}
