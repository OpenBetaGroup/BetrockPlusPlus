/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../command.h"
#include "../command_manager.h"
#include "../command_registry.h"
#include "commands/strategos.h"
#include "gamerules.h"
#include "server.h"
#include <cstddef>
#include <format>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

void SetGamerule(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto gameruleName = _cmd.get_arg<std::string>("name");
	auto valueText = _cmd.get_arg<std::string>("value");
	if (!gameruleName || Gamerule::ToId(*gameruleName) == Gamerule::Id::Invalid) {
		SendErrorFeedback(ctx, "Gamerule doesn't exist!");
		return;
	}
	if (!valueText) {
		SendErrorFeedback(ctx, "Missing value!");
		return;
	}

	const auto parsed = Gamerule::ParseLike(Gamerule::GetRaw(*gameruleName), *valueText);
	if (!parsed || !Gamerule::SetRaw(*gameruleName, *parsed)) {
		SendErrorFeedback(ctx, "Invalid value for this gamerule!");
		return;
	}

	SendFeedback(ctx, std::format("Gamerule {} is now: {}", *gameruleName, Gamerule::ValueToString(*parsed)));
	if (ctx.server && !ctx.server->PersistGamerule(*gameruleName))
		SendErrorFeedback(ctx, "Gamerule changed, but saving it to gamerules.txt failed!");
}

void GetGamerule(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	const auto gameruleName = _cmd.get_arg<std::string>("name");
	if (!gameruleName || Gamerule::ToId(*gameruleName) == Gamerule::Id::Invalid) {
		SendErrorFeedback(ctx, "Gamerule doesn't exist!");
		return;
	}
	const auto entry = Gamerule::GetEntry(*gameruleName);
	if (!entry) {
		SendErrorFeedback(ctx, "Gamerule doesn't exist!");
		return;
	}
	SendFeedback(ctx, std::format("Gamerule {} is: {}", *gameruleName, Gamerule::ValueToString(entry->value)));
	SendFeedback(ctx, std::format("§7{}", entry->description));
}

void ListGamerules(const strategos::CmdNode& /*_cmd*/, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	const auto gamerules = Gamerule::GetAllNames();
	SendFeedback(ctx, std::format("§7-- {} Gamerule(s) --", gamerules.size()));
	std::string line = "§7";
	size_t index = 0;
	for (const auto& [name, id] : gamerules) {
		if (line.size() + name.size() > 64) {
			SendFeedback(ctx, line);
			line = "§7";
		}
		line += name + ((index < (gamerules.size() - 1)) ? ", " : "");
		index++;
	}
	SendFeedback(ctx, line);
}

// LLM disclosure
std::vector<std::string> SuggestGamerules() {
	std::vector<std::string> names;
	for (const auto& [name, id] : Gamerule::GetAllNames())
		names.push_back(name);
	return names;
}
// End LLM disclosure

} // namespace

void RegisterGamerule(strategos::BrigadierContext& _dispatcher) {
	_dispatcher.add_command(strategos::Node::literal("gamerule")
	                            .describe("Lists, queries, or changes gamerules")
	                            .op()
	                            .then(strategos::Node::literal("list").executes(ListGamerules))
	                            .then(strategos::Node::string("name")
	                                      .suggests_from(SuggestGamerules)
	                                      .executes(GetGamerule)
	                                      .then(strategos::Node::string("value").executes(SetGamerule))));
}