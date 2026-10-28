/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#pragma once

#include "../player_conn/player_session.h"
#include "constants.h"
#include "inventory/item_stack.h"
#include "networking/packets.h"
#include "strategos.h"
#include "ucs2.h"
#include <optional>
#include <string_view>
#include <string>
#include <expected>
#include <vector>


#define ERROR_OPERATOR "Only operators can use this command!"
#define ERROR_PLAYER_REQUIRED "This command requires a player context. Use execute <target> <command>."
#define ERROR_PLAYER_NAME_REQUIRED "A player name is required from a non-player sender."
#define ERROR_RELATIVE_REQUIRES_PLAYER "Relative coordinates require a player context."
#define ERROR_CREATIVE "Only creative players can use this command!"
#define ERROR_PERMISSIONS "You lack the required permissions for this command!"
#define ERROR_WHITELIST "Only whitelisted players can use this command!"
#define ERROR_REASON_SYNTAX "Invalid Syntax"
#define ERROR_REASON_NO_EXIST "Command does not exist!"
#define ERROR_REASON_PARAMETERS "Invalid Parameters"
#define ERROR_REASON_TOO_FEW_PARAMETERS "Not enough parameters!"
#define ERROR_REASON_ERROR "Error"
#define ERROR_REASON_NO_CMD "No command passed"

#define MAX_CHAT_LINE_SIZE 60
// Classic client disconnects/crashes if a ChatMessage exceeds this UCS-2 length.
#define MAX_CHAT_MESSAGE_SIZE 119

class Server;
struct Entity;
struct CommandContext;

struct CommandTarget {
	Entity* entity = nullptr;
	PlayerSession* session = nullptr;
};

bool IsOperator(PlayerSession& _session, Server& _server);
bool IsOperator(const CommandContext& _ctx);
void SendCommandFeedback(PlayerSession* _recipient, const std::string& _message);
void SendFeedback(const CommandContext& _ctx, const std::string& _message);
ItemStack ParseItemStack(const std::string& _itemArg, std::optional<int> _count = std::nullopt);
void SendChunkedList(const CommandContext& _ctx, const std::string& _header,
                     const std::vector<std::string>& _entries);
std::optional<std::string> ResolveCmdTargetName(const strategos::CmdNode& _cmd, const CommandContext& _ctx);
std::expected<std::vector<CommandTarget>, std::string> ResolveCmdTargets(
    std::string_view _target, const CommandContext& _ctx);
std::expected<std::vector<CommandTarget>, std::string> ResolveCmdPlayerTargets(
    std::string_view _target, const CommandContext& _ctx, std::string_view _command);
EntityType GetEntityTypeFromString(std::string_view _name);
std::expected<Vec3, std::string> ResolveCmdPos(const CommandContext& _ctx, const strategos::Vec3& _position);

inline void SendChat(PlayerSession& _session, const std::string& _message) {
	std::u16string ucs2 = ToUCS2(_message);
	std::u16string colorPrefix;
	if (ucs2.size() >= 2 && ucs2[0] == u'\u00A7')
		colorPrefix = ucs2.substr(0, 2);

	size_t offset = 0;
	while (offset < ucs2.size()) {
		size_t chunk = MAX_CHAT_MESSAGE_SIZE;
		std::u16string line;
		if (offset > 0 && !colorPrefix.empty()) {
			line = colorPrefix;
			chunk = MAX_CHAT_MESSAGE_SIZE > colorPrefix.size() ? MAX_CHAT_MESSAGE_SIZE - colorPrefix.size()
			                                                   : MAX_CHAT_MESSAGE_SIZE;
		}
		line += ucs2.substr(offset, chunk);
		const size_t consumed = chunk < ucs2.size() - offset ? chunk : ucs2.size() - offset;
		offset += consumed;

		Packet::ChatMessage pkt;
		pkt.message = ToUTF8(line);
		pkt.Serialize(_session.stream);
	}
}

inline Vec3 ResolveCmdVec3(const strategos::Vec3& _cmd, const Vec3& _origin) {
	Vec3 out{ static_cast<double>(_cmd.pos[0]), static_cast<double>(_cmd.pos[1]), static_cast<double>(_cmd.pos[2]) };
	if (_cmd.is_relative(strategos::Axis::X))
		out.x += _origin.x;
	if (_cmd.is_relative(strategos::Axis::Y))
		out.y += _origin.y;
	if (_cmd.is_relative(strategos::Axis::Z))
		out.z += _origin.z;
	return out;
}

inline void SendTeleport(PlayerSession& _target, Vec3 _position, float _yaw = 0.0f, float _pitch = 0.0f) {
	_target.entity->Teleport(_position, { _yaw, _pitch });

	_target.pendingTeleport = _position;
	_target.position.pos = _position;
	_target.pendingPosition.reset();

	Packet::PlayerPositionAndRotation pkt;
	pkt.position.x = _position.x;
	pkt.position.y = _position.y + PLAYER_EYE_HEIGHT;
	pkt.cameraY = _position.y;
	pkt.position.z = _position.z;
	pkt.yaw = _yaw;
	pkt.pitch = _pitch;
	pkt.onGround = false;
	pkt.Serialize(_target.stream);
}
