/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../command.h"
#include "../command_manager.h"
#include "../command_registry.h"
#include "platform/platform_label.h"
#include "version.h"
#include <format>

namespace {

std::string ShowAbout(const strategos::CmdNode&, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	SendFeedback(ctx, std::format("§e----- [   {} v{}   ] -----", PROJECT_NAME, PROJECT_VERSION_FULL_STRING));
	SendFeedback(ctx, std::string{ "Open-source Beta 1.7.3 server written in C++23" });
	SendFeedback(ctx, std::string{ "https://github.com/OpenBetaGroup/BetrockPlusPlus" });
	SendFeedback(ctx, std::string{ "§7Licensed under the GNU AGPL v3.0 (AGPL-3.0-only)" });
	SendFeedback(ctx, std::format("§7Compiled for {} ({}) in {} mode", PLATFORM_NAME, ARCH_NAME, BUILD_MODE));
	return "";
}

} // namespace

void RegisterAbout(strategos::BrigadierContext& _dispatcher) {
	_dispatcher.add_command(strategos::Node::literal("about").describe("Shows project information").executes(ShowAbout));
}
