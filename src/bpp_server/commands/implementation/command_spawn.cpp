/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../command.h"
#include "../command_manager.h"
#include "../command_registry.h"
#include "commands/strategos.h"
#include "constants.h"
#include "dimensions.h"
#include "gamerules.h"
#include "numeric_structs.h"
#include "player_conn/player_session_mp.h"
#include "server.h"
#include "world.h"
#include "world/dimension/dimension.h"
#include <memory>
#include <optional>

namespace {

void TeleportToSpawn(const strategos::CmdNode&, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	if (!ctx.session || !ctx.session->entity) {
		SendErrorFeedback(ctx, ERROR_PLAYER_REQUIRED);
		return;
	}
	if (!ctx.world) {
		SendErrorFeedback(ctx, ERROR_REASON_ERROR);
		return;
	}

	WorldManager* overworld = ctx.server->GetWorldForDimension(Dimension::Overworld);
	Int32_3 ipos = overworld->GetSpawnPoint(false);

	bool hasSpawnChunksEnabled = Gamerule::Get<bool>("spawn_chunks");

	// We can do a generic tp since the chunk is guaranteed to be loaded
	if (hasSpawnChunksEnabled) {
		Vec3 dest{ double(ipos.x) + 0.5, double(overworld->GetHeightValue(ipos.x, ipos.z)) + 0.01,
			       double(ipos.z) + 0.5 };
		if (ctx.world->dimension->id != Dimension::Overworld) {
			ctx.server->SendPlayerToDimension(Dimension::Overworld, *ctx.session, dest);
			return;
		}

		SendTeleport(*ctx.session, dest);
		return;
	}

	// Fallback
	Vec2 dest{ double(ipos.x) + 0.5, double(ipos.z) + 0.5 };
	if (ctx.world->dimension->id != Dimension::Overworld) {
		Vec3 tpPos = { dest.x, CHUNK_HEIGHT + 1.0, dest.z };
		ctx.server->SendPlayerToDimension(Dimension::Overworld, *ctx.session, tpPos);
		SendTeleport2d(*ctx.session, dest);
		return;
	}
	SendTeleport2d(*ctx.session, dest);
}

void SetSpawnPos(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto cmdPos = _cmd.get_arg<strategos::Vec3>("pos");
	if (!cmdPos) {
		SendErrorFeedback(ctx, ERROR_REASON_PARAMETERS);
		return;
	}
	if (!ctx.world) {
		SendErrorFeedback(ctx, ERROR_REASON_ERROR);
		return;
	}
	auto resolved = ResolveCmdPos(ctx, *cmdPos);
	if (!resolved) {
		SendErrorFeedback(ctx, resolved.error());
		return;
	}
	Vec3 pos = *resolved;

	WorldManager* overworld = ctx.server->GetWorldForDimension(Dimension::Overworld);
	if (!overworld) {
		SendErrorFeedback(ctx, ERROR_REASON_ERROR);
		return;
	}
	overworld->spawnPoint.x = pos.x;
	overworld->spawnPoint.z = pos.z;
	SendFeedback(ctx, std::format("Set world spawn to {}", overworld->GetSpawnPoint(false).Str()));
}

void GetSpawn(const strategos::CmdNode&, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	if (!ctx.world) {
		SendErrorFeedback(ctx, ERROR_REASON_ERROR);
		return;
	}

	WorldManager* overworld = ctx.server->GetWorldForDimension(Dimension::Overworld);
	Int32_3 ipos = overworld->GetSpawnPoint(false);
	SendFeedback(ctx, "World spawn is at " + Int32_2{ipos.x,ipos.z}.Str());
}

} // namespace

void RegisterSpawn(strategos::BrigadierContext& _dispatcher) {
	_dispatcher.add_command(
		strategos::Node::literal("spawn").describe("Teleport to, get and set spawn").executes(TeleportToSpawn)
		.op()
		.then(strategos::Node::literal("set").describe("Set world spawn at current x,z position")
			.then(strategos::Node::vec3("pos").describe("Set world spawn at passed x,z position (requires x,y,z)").executes(SetSpawnPos)))
		.then(strategos::Node::literal("get").describe("Get world spawn").executes(GetSpawn))
	);
}
