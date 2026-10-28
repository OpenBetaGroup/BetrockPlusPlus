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

namespace {

std::string GiveItem(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	if (!ctx.session) return ERROR_PLAYER_REQUIRED;
	auto itemArg = _cmd.get_arg<std::string>("item");
	if (!itemArg)
		return "Missing item id!";

	std::optional<int> count;
	if (auto amount = _cmd.get_arg<int>("amount"))
		count = *amount;

	ItemStack item;
	try {
		item = ParseItemStack(*itemArg, count);
	} catch (...) {
		return ERROR_REASON_PARAMETERS;
	}

	if (!Items::IsValidId(item.id))
		return std::to_string(item.id) + " is not a valid item id!";

	SendFeedback(ctx, "§eGave " + IdToLabel(item.id) + " (" + std::to_string(item.id) + ":" +
	                           std::to_string(item.data) + ") x" + std::to_string(item.count) + " to " +
	                           ctx.session->username);

	if (ctx.session->inventory.PickupItem(item))
		PacketUtilities::SendInventory(*ctx.session, ctx.session->openWindowId, ctx.session->inventory);
	return "";
}

std::string ClearOwnInventory(const strategos::CmdNode& /*_cmd*/, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	if (!ctx.session) return ERROR_PLAYER_REQUIRED;
	const auto count = ctx.session->inventory.Clear();
	PacketUtilities::SendInventory(*ctx.session, 0, ctx.session->inventory);
	SendFeedback(ctx, std::format("§eRemoved {} Item(s) from player {}", count, ctx.session->username));
	return "";
}

std::string ClearOtherInventory(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto playerName = _cmd.get_arg<std::string>("player");
	if (!playerName) return ERROR_REASON_PARAMETERS;
	if (!IsOperator(ctx)) return ERROR_OPERATOR;
	if (!playerName->empty() && playerName->front() == '@') {
		auto targets = ResolveCmdPlayerTargets(*playerName, ctx, "Clear");
		if (!targets) return targets.error();

		for (const CommandTarget& resolved : *targets) {

			auto& target = *resolved.session;
			const auto count = target.inventory.Clear();
			PacketUtilities::SendInventory(target, 0, target.inventory);
			SendFeedback(ctx, std::format("§eRemoved {} Item(s) from player {}", count, target.username));
		}
		return "";
	}
	auto target = ctx.server->GetSessionByUsername(*playerName);
	if (!target) return *playerName + " does not exist!";
	const auto count = target->inventory.Clear();
	PacketUtilities::SendInventory(*target, 0, target->inventory);
	SendFeedback(ctx, std::format("§eRemoved {} Item(s) from player {}", count, *playerName));
	return "";
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
	                            .executes(ClearOwnInventory)
	                            .then(strategos::Node::string("player")
	                                      .describe("Target inventory to clear")
	                                      .op()
	                                      .suggests(strategos::SuggestionSource::Players)
	                                      .executes(ClearOtherInventory)));
}
