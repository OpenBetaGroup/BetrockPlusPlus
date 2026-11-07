/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../command.h"
#include "../command_manager.h"
#include "../command_registry.h"
#include "networking/packets.h"

namespace {

std::string TeleportToSpawn(const strategos::CmdNode&, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	if (!ctx.session || !ctx.session->entity) return ERROR_PLAYER_REQUIRED;
	if (!ctx.world) return ERROR_REASON_ERROR;

	WorldManager* overworld = ctx.server->GetWorldForDimension(Dimension::Overworld);
	Int32_3 ipos = overworld->GetSpawnPoint(false);

	bool hasSpawnChunksEnabled = Gamerule::Get<bool>("spawn_chunks");

	// We can do a generic tp since the chunk is guaranteed to be loaded
	if (hasSpawnChunksEnabled) {
		Vec3 dest{ double(ipos.x) + 0.5, double(overworld->GetHeightValue(ipos.x, ipos.z)) + 0.01, double(ipos.z) + 0.5 };
		if (ctx.world->thisDimension != Dimension::Overworld) {
			ctx.server->SendPlayerToDimension(Dimension::Overworld, *ctx.session, dest);
			return "";
		}

		SendTeleport(*ctx.session, dest);
		return "";
	}

	// Fallback
	Vec2 dest{ double(ipos.x) + 0.5, double(ipos.z) + 0.5 };
	if (ctx.world->thisDimension != Dimension::Overworld) {
		Vec3 tpPos = { dest.x, CHUNK_HEIGHT + 1.0, dest.z };
		ctx.server->SendPlayerToDimension(Dimension::Overworld, *ctx.session, tpPos);
		SendTeleport2d(*ctx.session, dest);
		return "";
	}
	SendTeleport2d(*ctx.session, dest);
	return "";
}

} // namespace

void RegisterSpawn(strategos::BrigadierContext& _dispatcher) {
	_dispatcher.add_command(strategos::Node::literal("spawn").describe("Teleport to spawn").executes(TeleportToSpawn));
}
