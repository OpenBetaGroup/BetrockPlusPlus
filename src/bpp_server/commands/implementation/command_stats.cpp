/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../../../bpp_shared/platform/memory.h"
#include "../command.h"
#include "../command_manager.h"
#include "../command_registry.h"
#include "chunk.h"
#include "server.h"
#include <algorithm>
#include <array>
#include <format>

namespace {

	std::string GenerateUsageBar(double _total, double _active, double _overworld, double _nether) {
		std::string bar = "§f[";
		if (_total <= 0.0) {
			return bar + std::string(30, ' ') + "§f]";
		}
		// Clamp values to valid ranges.
		double total = _total;
		double active = std::clamp(_active, 0.0, total);
		double overworld = std::clamp(_overworld, 0.0, active);
		double nether = std::clamp(_nether, 0.0, active - overworld);
		// Calculate cumulative boundaries.
		double overworldEnd = overworld / total;
		double netherEnd = (overworld + nether) / total;
		double activeEnd = active / total;

		for (int i = 0; i < 30; i++) {
			double position = (i + 0.5) / 30.0;
			if (position < overworldEnd) {
				bar += "§a#"; // Overworld
			} else if (position < netherEnd) {
				bar += "§c#"; // Nether
			} else if (position < activeEnd) {
				bar += "§e#"; // Other active usage
			} else {
				bar += "§0#"; // Inactive
			}
		}
		bar += "§f]";
		return bar;
	}

	std::string ShowStats(const strategos::CmdNode&, void* _userData) {
		auto& ctx = CmdCtx(_userData);
		const auto& players = ctx.server->GetPlayers();
		const WorldManager& overworld = ctx.server->gameRuntime.world;
		const WorldManager& nether = ctx.server->gameRuntime.worldHell;
		const size_t chunksOverworld = overworld.chunks.size();
		const size_t chunksNether = nether.chunks.size();
		auto sumChunkBytes = [](const auto& _chunks) {
			size_t bytes = 0;
			for (const auto& entry : _chunks) {
				if (entry.second)
					bytes += entry.second->GetMemoryUsage();
			}
			return bytes;
		};
		const double chunksOverworldMb = static_cast<double>(sumChunkBytes(overworld.chunks)) / 1024.0 / 1024.0;
		const double chunksNetherMb = static_cast<double>(sumChunkBytes(nether.chunks)) / 1024.0 / 1024.0;
		const double totalUsageMb = GetMemoryUsage(MemoryUnit::Megabyte);
		const double activeUsageMb = GetActiveMemoryUsage(MemoryUnit::Megabyte);

		// Tick performance
		const double maxTps = ctx.server->GetTickRate();
		const double avgTps = ctx.server->averageTickPerSecond;
		const double mspt = ctx.server->averageTickMs;
		const double budgetMs = maxTps > 0.0 ? 1000.0 / maxTps : 0.0;
		const double loadPct = budgetMs > 0.0 ? mspt / budgetMs * 100.0 : 0.0;
		const double maxSustainableTps = mspt > 0.0 ? 1000.0 / mspt : maxTps;

		const std::array<std::string, 7> lines = {
			std::format("§7Alloc: {}", GenerateUsageBar(totalUsageMb, activeUsageMb, chunksOverworldMb, chunksNetherMb)),
			std::format("§7Total: {:.2f} MB, Active: {:.2f} MB", totalUsageMb, activeUsageMb),
			std::format("§7World: {} (~{:.1f} MB), Hell: {} (~{:.1f} MB)", chunksOverworld, chunksOverworldMb,
						chunksNether, chunksNetherMb),
						std::format("§7{} Players, {} Entities, {} Hell Entities", players.size(),
									ctx.server->overworldEntityTracker.trackedEntities.size(),
									ctx.server->hellEntityTracker.trackedEntities.size()),
									std::format("§7Avg. TPS: {:.2f}/{:.2f} ({:+.2f})", avgTps, maxTps, avgTps - maxTps),
									std::format("§7Avg. MSPT: {:.2f} ms ({:.0f}% of {:.0f} ms budget)", mspt, loadPct, budgetMs),
									std::format("§7Headroom: {:+.2f} ms, max sustainable ~{:.1f} TPS", budgetMs - mspt, maxSustainableTps)
		};
		for (const auto& line : lines)
			SendFeedback(ctx, line);
		return "";
	}

} // namespace

void RegisterStats(strategos::BrigadierContext& _dispatcher) {
	_dispatcher.add_command(strategos::Node::literal("stats").describe("Shows usage statistics").executes(ShowStats));
}
