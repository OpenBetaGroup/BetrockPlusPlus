/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../command.h"
#include "../command_manager.h"
#include "../command_registry.h"
#include "addon/addon.h"
#include "addon/addon_manager.h"
#include "commands/strategos.h"
#include "server.h"
#include <format>
#include <memory>
#include <string>
#include <vector>

namespace {
void ListAddons(const strategos::CmdNode&, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	const auto& addons = ctx.server->GetAddonManager().GetAddons();
	std::vector<std::string> addonLabels;
	addonLabels.reserve(addons.size());
	for (const auto& addon : addons) {
		if (!addon)
			continue;
		addonLabels.push_back(std::format("{} ({}, {})", addon->info.name, addon->info.id, addon->info.version));
	}
	SendChunkedList(ctx, std::format("§7-- {} Active Addon(s) --", addons.size()), addonLabels);
}

// TODO: Add reload addon
// TODO: Add enable addon
// TODO: Add disable addon

} // namespace

void RegisterAddon(strategos::BrigadierContext& _dispatcher) {
	_dispatcher.add_command(strategos::Node::literal("addon")
	                            .describe("Addon managing commands")
	                            .op()
	                            .then(strategos::Node::literal("list").executes(ListAddons)));
}
