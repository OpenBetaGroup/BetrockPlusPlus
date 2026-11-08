/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../command.h"
#include "../command_manager.h"
#include "../command_registry.h"
#include "blocks.h"
#include "server.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <format>

namespace {

std::string FillArea(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto blockArg = _cmd.get_arg<std::string>("block");
	auto fromCmd = _cmd.get_arg<strategos::Vec3>("from");
	auto toCmd = _cmd.get_arg<strategos::Vec3>("to");
	if (!blockArg || !fromCmd || !toCmd)
		return ERROR_REASON_PARAMETERS;

	ItemStack item;
	try {
		item = ParseItemStack(*blockArg);
	} catch (...) {
		return ERROR_REASON_PARAMETERS;
	}

	if (item.id >= BLOCK_MAX || item.id < 0)
		return "Invalid Block Id!";

	auto fromResult = ResolveCmdPos(ctx, *fromCmd);
	if (!fromResult)
		return fromResult.error();
	auto toResult = ResolveCmdPos(ctx, *toCmd);
	if (!toResult)
		return toResult.error();
	Vec3 from = *fromResult;
	Vec3 to = *toResult;
	Int3 pos0{ static_cast<int32_t>(std::floor(from.x)), static_cast<int32_t>(std::floor(from.y)),
		       static_cast<int32_t>(std::floor(from.z)) };
	Int3 pos1{ static_cast<int32_t>(std::floor(to.x)), static_cast<int32_t>(std::floor(to.y)),
		       static_cast<int32_t>(std::floor(to.z)) };

	if (pos0.y >= CHUNK_HEIGHT || pos0.y < 0 || pos1.y >= CHUNK_HEIGHT || pos1.y < 0)
		return ERROR_REASON_PARAMETERS;

	// Normalize
	Int3 minPos{ std::min(pos0.x, pos1.x), std::min(pos0.y, pos1.y), std::min(pos0.z, pos1.z) };
	Int3 maxPos{ std::max(pos0.x, pos1.x), std::max(pos0.y, pos1.y), std::max(pos0.z, pos1.z) };

	if (!ctx.world->AABBinValidChunks({ double(minPos.x), double(minPos.y), double(minPos.z), double(maxPos.x),
	                                    double(maxPos.y), double(maxPos.z) }))
		return "Tried to fill in unloaded chunks!";

	int64_t width = int64_t(maxPos.x - minPos.x) + 1;
	int64_t height = int64_t(maxPos.y - minPos.y) + 1;
	int64_t depth = int64_t(maxPos.z - minPos.z) + 1;
	int64_t volume = width * height * depth;

	SendFeedback(ctx, std::format("Attempting to fill {} block(s)...", volume));

	auto fillStart = std::chrono::steady_clock::now();
	ctx.world->FillVolume(minPos, maxPos, static_cast<BlockType>(item.id.value), static_cast<uint8_t>(item.data));
	float fillSeconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - fillStart).count();
	SendFeedback(ctx, std::format("Filled {} block(s) in {:.2f} seconds!", volume, fillSeconds));
	return "";
}

} // namespace

void RegisterFill(strategos::BrigadierContext& _dispatcher) {
	_dispatcher.add_command(strategos::Node::literal("fill")
	                            .describe("Fills an area with the desired block")
	                            .op()
	                            .then(strategos::Node::string("block").then(strategos::Node::vec3("from").then(
	                                strategos::Node::vec3("to").executes(FillArea)))));
}
