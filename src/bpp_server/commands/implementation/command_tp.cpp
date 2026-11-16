/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../command.h"
#include "../command_manager.h"
#include "../command_registry.h"
#include "server.h"
#include <algorithm>

namespace {

constexpr int K_TELEPORT_LIMIT = 2147482000;

void ClampTeleport(Vec3& _pos) {
	_pos.x = std::clamp(static_cast<int>(_pos.x), -K_TELEPORT_LIMIT, K_TELEPORT_LIMIT);
	_pos.y = std::clamp(static_cast<int>(_pos.y), -K_TELEPORT_LIMIT, K_TELEPORT_LIMIT);
	_pos.z = std::clamp(static_cast<int>(_pos.z), -K_TELEPORT_LIMIT, K_TELEPORT_LIMIT);
}

std::string EntityName(const CommandTarget& _target) {
	if (_target.session)
		return _target.session->username;
	return "entity " + std::to_string(_target.entity->id);
}

void TeleportToCoords(const CommandTarget& _source, const CommandContext& _ctx, const strategos::Vec3& _cmdPos) {
	Entity* entity = _source.entity;
	if (!entity || entity->isDead) {
		SendErrorFeedback(_ctx, "Teleport source does not exist!");
		return;
	}
	CommandContext sourceContext = _ctx;
	sourceContext.executionEntity = entity;
	sourceContext.session = _source.session;
	auto resolved = ResolveCmdPos(sourceContext, _cmdPos);
	if (!resolved) {
		SendErrorFeedback(_ctx, resolved.error());
		return;
	}
	Vec3 pos = *resolved;
	ClampTeleport(pos);
	if (_source.session && _source.session->entity)
		SendTeleport(*_source.session, pos);
	else
		entity->Teleport(pos);
	SendFeedback(_ctx, "§eTeleported " + EntityName(_source) + " to " + pos.Str());
}

void TpSelfCoords(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	if (!ctx.session || !ctx.session->entity) {
		SendErrorFeedback(ctx, ERROR_PLAYER_REQUIRED);
		return;
	}
	auto pos = _cmd.get_arg<strategos::Vec3>("pos");
	if (!pos) {
		SendErrorFeedback(ctx, ERROR_REASON_PARAMETERS);
		return;
	}
	TeleportToCoords({ ctx.session->entity.get(), ctx.session }, ctx, *pos);
}

void TpPlayerCoords(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto sourceArg = _cmd.get_arg<std::string>("player");
	auto pos = _cmd.get_arg<strategos::Vec3>("pos");
	if (!sourceArg || !pos) {
		SendErrorFeedback(ctx, ERROR_REASON_PARAMETERS);
		return;
	}

	auto sources = ResolveCmdTargets(*sourceArg, ctx);
	if (!sources) {
		SendErrorFeedback(ctx, sources.error());
		return;
	}
	for (const CommandTarget& source : *sources)
		TeleportToCoords(source, ctx, *pos);
}

void TpPlayerTarget(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto sourceArg = _cmd.get_arg<std::string>("player");
	auto targetArg = _cmd.get_arg<std::string>("target");
	if (!sourceArg || !targetArg) {
		SendErrorFeedback(ctx, ERROR_REASON_PARAMETERS);
		return;
	}
	auto sources = ResolveCmdTargets(*sourceArg, ctx);
	if (!sources) {
		SendErrorFeedback(ctx, sources.error());
		return;
	}
	auto destinations = ResolveCmdTargets(*targetArg, ctx);
	if (!destinations) {
		SendErrorFeedback(ctx, destinations.error());
		return;
	}

	for (const CommandTarget& source : *sources) {
		if (!source.entity || source.entity->isDead)
			continue;
		for (const CommandTarget& destination : *destinations) {
			if (!destination.entity || destination.entity->isDead)
				continue;

			const Vec3 position{ destination.entity->position.x, destination.entity->position.y + 0.01,
				                 destination.entity->position.z };
			const Vec2 rotation{ destination.entity->rotationYaw, destination.entity->rotationPitch };

			if (source.session && source.session->entity)
				SendTeleport(*source.session, position, rotation.x, rotation.y);
			else
				source.entity->Teleport(position, rotation);
			SendFeedback(ctx, "§eTeleported " + EntityName(source) + " to " + EntityName(destination));
		}
	}
}

} // namespace

void RegisterTeleport(strategos::BrigadierContext& _dispatcher) {
	_dispatcher.add_command(strategos::Node::literal("tp")
	                            .describe("Teleports players & entities to coordinates or another target")
	                            .op()
	                            .then(strategos::Node::vec3("pos").executes(TpSelfCoords))
	                            .then(strategos::Node::string("player")
	                                      .suggests(strategos::SuggestionSource::Players)
	                                      .then(strategos::Node::vec3("pos").executes(TpPlayerCoords))
	                                      .then(strategos::Node::string("target")
	                                                .suggests(strategos::SuggestionSource::Players)
	                                                .executes(TpPlayerTarget))));
}
