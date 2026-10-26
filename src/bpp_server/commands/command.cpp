/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "command.h"
#include "../../bpp_shared/helpers/identifiers.h"
#include "../server.h"
#include "username.h"
#include "items.h"
#include "command_manager.h"
#include <algorithm>
#include <string>

bool IsOperator(PlayerSession& _session, Server& _server) {
	return ContainsUsername(_server.operatorUsernames, _session.username);
}

bool IsOperator(const CommandContext& _ctx) {
	return !_ctx.sender || (_ctx.server && IsOperator(*_ctx.sender, *_ctx.server));
}

void SendCommandFeedback(PlayerSession* _recipient, const std::string& _message) {
	if (_recipient) {
		if (!_recipient->stream.IsConnected())
			return;
		SendChat(*_recipient, _message);
		return;
	}
	Logger::LogStyled(_message);
}

void SendFeedback(const CommandContext& _ctx, const std::string& _message) {
	SendCommandFeedback(_ctx.sender, _message);
}

ItemStack ParseItemStack(const std::string& _itemArg, std::optional<int> _count) {
	ItemStack item;

	if (auto it = IDENTIFIER_TO_ID.find(_itemArg); it != IDENTIFIER_TO_ID.end()) {
		item.id = it->second.id;
		item.data = it->second.meta;
	} else {
		const size_t colonPos = _itemArg.find(':');
		const std::string baseString = _itemArg.substr(0, colonPos);

		std::optional<int16_t> explicitMeta;
		if (colonPos != std::string::npos) {
			const std::string metaString = _itemArg.substr(colonPos + 1);
			if (!metaString.empty()) {
				explicitMeta = static_cast<int16_t>(std::stoi(metaString));
			}
		}

		if (auto it2 = IDENTIFIER_TO_ID.find(baseString); it2 != IDENTIFIER_TO_ID.end()) {
			item.id = it2->second.id;
			item.data = it2->second.meta;
		} else {
			item.id = static_cast<int16_t>(std::stoi(baseString));
			item.data = 0;
		}

		// Explicit meta, if given, overrides the default
		if (explicitMeta) {
			item.data = *explicitMeta;
		}
	}

	item.count = _count ? static_cast<int8_t>(*_count) : Items::GetMaxStack(item.id);
	return item;
}

void SendChunkedList(const CommandContext& _ctx, const std::string& _header,
                     const std::vector<std::string>& _entries) {
	SendFeedback(_ctx, _header);
	std::string line = "§7";
	for (size_t i = 0; i < _entries.size(); i++) {
		const auto& entry = _entries[i];
		const std::string suffix = i + 1 < _entries.size() ? ", " : "";
		if (line.size() + entry.size() + suffix.size() > 64 && line != "§7") {
			SendFeedback(_ctx, line);
			line = "§7";
		}
		line += entry + suffix;
	}
	if (line != "§7")
		SendFeedback(_ctx, line);
}

std::optional<std::string> ResolveCmdTargetName(const strategos::CmdNode& _cmd, const CommandContext& _ctx) {
	if (auto name = _cmd.get_arg<std::string>("username"))
		return name;
	if (_ctx.session)
		return _ctx.session->username;
	return std::nullopt;
}

std::expected<Vec3, std::string> ResolveCmdPos(const CommandContext& _ctx,
                                                         const strategos::Vec3& _position) {
	const bool relative = _position.is_relative(strategos::Axis::X) ||
	                      _position.is_relative(strategos::Axis::Y) ||
	                      _position.is_relative(strategos::Axis::Z);
	if (relative && !_ctx.session)
		return std::unexpected(ERROR_RELATIVE_REQUIRES_PLAYER);
	return ResolveCmdVec3(_position, _ctx.session ? _ctx.session->position.pos : Vec3{});
}

