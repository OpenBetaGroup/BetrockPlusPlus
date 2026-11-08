/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include "logger.h"
#include "world/storage/region_manager.h"
#include "world/world.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <climits>
#include <cstdint>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// Overworld chunk generation speed test.
//
// Usage:  ChunkBenchmark::PerformChunkGenSpeedTest(gameRuntime.world);
//
// For every seed in GEN_TEST_SEEDS the overworld is generated GEN_TEST_RUNS_PER_SEED times at render distance
// GEN_TEST_RENDER_DISTANCE (a 41x41 square of chunks centred on 0,0). One "run" is one finished world: every chunk
// generated, every populatable chunk populated, and the light queue drained. min / max / average are reported
// per finished world.
//
// This is purely terrain generation churn: nothing is saved or loaded, and no entities are created or touched.
// The world is swapped over to an empty, uninitialised RegionManager (no folder, so no chunk ever "exists" on
// disk and everything gets generated) and nothing is ever written. The world's seed, chunks and region manager
// are put back afterwards. It still shares the world's thread pool and light queue, so call it at startup before
// players are connected.
namespace ChunkBenchmark {

inline constexpr int GEN_TEST_RENDER_DISTANCE = 20;
inline constexpr int GEN_TEST_RUNS_PER_SEED = 3;
// Untimed worlds generated first so cold-start costs (page faults, allocator growth, first use of the generators)
// don't land in the min/max. Set to 0 to disable.
inline constexpr int GEN_TEST_WARMUP_RUNS = 1;
// Give up on a world if it makes no progress for this long, so a stuck pipeline can't hang startup forever.
inline constexpr double GEN_TEST_STALL_TIMEOUT_SECONDS = 60.0;

inline const std::array<std::string, 8> GEN_TEST_SEEDS = {
	"404",
	"3257840388504953787", // Pack.png
	"Gargamel",
	"Glacier",
	"2409838883250561605",
	"worstseedever",
	"JASON",
	"Doom",
};

// Timings for a set of finished worlds.
struct GenSpeedStats {
	double minTime = std::numeric_limits<double>::max(); // seconds, fastest finished world
	double maxTime = 0.0;                                // seconds, slowest finished world
	double totalTime = 0.0;                              // seconds, sum of every finished world
	int worlds = 0;                                      // number of finished worlds
	int chunksPerWorld = 0;

	void Add(double _seconds) {
		minTime = std::min(minTime, _seconds);
		maxTime = std::max(maxTime, _seconds);
		totalTime += _seconds;
		worlds++;
	}

	double AverageTime() const {
		return worlds ? totalTime / worlds : 0.0;
	}

	double ChunksPerSecond() const {
		return totalTime > 0.0 ? double(chunksPerWorld) * worlds / totalTime : 0.0;
	}
};

struct GenSpeedResult {
	GenSpeedStats overall;
	std::vector<std::pair<std::string, GenSpeedStats>> perSeed;
	bool completed = false; // false if the test bailed out early (setup failure or stalled pipeline)
};

namespace Detail {

// Stashes the parts of the world the test is about to clobber, swaps in an empty region manager, and puts
// everything back when it goes out of scope (including on early return).
class WorldStateGuard {
public:
	WorldStateGuard(WorldManager& _world, RegionManager& _emptyRegions)
	    : world(_world), savedRegions(_world.regionManager), savedSeed(_world.seed),
	      savedChunks(std::move(_world.chunks)) {
		world.chunks.clear(); // A moved-from map is only guaranteed to be valid, not empty
		world.InvalidateChunkCache();
		world.regionManager = &_emptyRegions;
	}

	~WorldStateGuard() {
		world.pool.wait();
		world.chunks.clear();
		world.chunks = std::move(savedChunks);
		world.regionManager = savedRegions;
		world.InitWorldSeed(savedSeed); // Also restores the shared biome generator
		world.InvalidateChunkCache();
	}

	WorldStateGuard(const WorldStateGuard&) = delete;
	WorldStateGuard& operator=(const WorldStateGuard&) = delete;

private:
	WorldManager& world;
	RegionManager* savedRegions;
	int64_t savedSeed;
	std::unordered_map<Int32_2, std::shared_ptr<Chunk>> savedChunks;
};

// Generates one finished world at `_radius` and returns how long it took in seconds, or nullopt if the pipeline
// stalled. The world's chunk map is left populated; the caller clears it.
inline std::optional<double> GenerateFinishedWorld(WorldManager& _world, int _radius) {
	using Clock = std::chrono::steady_clock;

	const int diameter = _radius * 2 + 1;
	const int totalChunks = diameter * diameter;
	// A chunk can only be populated once its +X, +Z and +X+Z neighbours exist, so the outer +X/+Z edge never is.
	const int populatableChunks = (diameter - 1) * (diameter - 1);

	// Request every chunk in the square
	for (int dx = -_radius; dx <= _radius; dx++) {
		for (int dz = -_radius; dz <= _radius; dz++) {
			Int32_2 pos{ dx, dz };
			auto chunk = std::make_shared<Chunk>();
			chunk->cpos = pos;
			_world.chunks.emplace(pos, std::move(chunk));
		}
	}

	auto start = Clock::now();
	auto lastProgress = start;
	int lastProgressCount = -1;

	// Same pump the server uses for spawn chunks, minus the logging.
	while (true) {
		_world.PumpPipeline({});
		_world.pool.wait();
		_world.DrainGenQueue();
		_world.regionManager->iopool.wait();
		_world.DrainLoadQueue();
		_world.PopulateReady(INT_MAX);
		_world.lightManager.ProcessLightQueue(_world);

		int generated = 0;
		int populated = 0;
		for (auto& [pos, chunk] : _world.chunks) {
			if (chunk->state.load() >= ChunkState::Generated)
				generated++;
			if (chunk->isTerrainPopulated)
				populated++;
		}

		if (generated >= totalChunks && populated >= populatableChunks)
			break;

		auto now = Clock::now();
		if (generated + populated != lastProgressCount) {
			lastProgressCount = generated + populated;
			lastProgress = now;
		} else if (std::chrono::duration<double>(now - lastProgress).count() > GEN_TEST_STALL_TIMEOUT_SECONDS) {
			Logger::Error << std::format("Chunk gen test stalled with {}/{} generated and {}/{} populated.\n",
			                             generated, totalChunks, populated, populatableChunks);
			return std::nullopt;
		}
	}

	return std::chrono::duration<double>(Clock::now() - start).count();
}

// Throw away every chunk so the next run starts from nothing.
inline void DeleteAllChunks(WorldManager& _world) {
	_world.pool.wait();
	_world.chunks.clear();
	_world.InvalidateChunkCache();
}

inline void LogStats(const std::string& _label, const GenSpeedStats& _stats) {
	Logger::Info << std::format("{} min {:.3f}s | max {:.3f}s | avg {:.3f}s | {:.0f} chunks/s ({} worlds)\n", _label,
	                            _stats.minTime, _stats.maxTime, _stats.AverageTime(), _stats.ChunksPerSecond(),
	                            _stats.worlds);
}

} // namespace Detail

inline GenSpeedResult PerformChunkGenSpeedTest(WorldManager& _world) {
	GenSpeedResult result;
	if (_world.GetDimension() != Dimension::Overworld) {
		Logger::Warning << "Chunk gen test only supports the overworld, skipping.\n";
		return result;
	}

	// Never initialised, so it has no folder and ChunkExists() is always false: every chunk gets generated.
	RegionManager emptyRegions;
	emptyRegions.world = &_world;
	// Destroyed before emptyRegions, so the world never points at a dead manager
	Detail::WorldStateGuard guard(_world, emptyRegions);

	const int radius = GEN_TEST_RENDER_DISTANCE;
	const int chunksPerWorld = (radius * 2 + 1) * (radius * 2 + 1);
	result.overall.chunksPerWorld = chunksPerWorld;

	Logger::Info << std::format("Chunk gen test: {} seeds x {} worlds at render distance {} ({} chunks each)\n",
	                            GEN_TEST_SEEDS.size(), GEN_TEST_RUNS_PER_SEED, radius, chunksPerWorld);

	bool ok = true;

	// Warmup, not counted
	for (int i = 0; ok && i < GEN_TEST_WARMUP_RUNS; i++) {
		Logger::Info << "Warming up...\n";
		_world.InitWorldSeed(HashCode(GEN_TEST_SEEDS[0]));
		ok = Detail::GenerateFinishedWorld(_world, radius).has_value();
		Detail::DeleteAllChunks(_world);
	}

	for (std::string seed : GEN_TEST_SEEDS) {
		if (!ok)
			break;
		GenSpeedStats seedStats;
		seedStats.chunksPerWorld = chunksPerWorld;
		_world.InitWorldSeed(HashCode(seed));

		for (int run = 1; run <= GEN_TEST_RUNS_PER_SEED; run++) {
			auto seconds = Detail::GenerateFinishedWorld(_world, radius);
			Detail::DeleteAllChunks(_world);
			if (!seconds) {
				ok = false;
				break;
			}
			seedStats.Add(*seconds);
			result.overall.Add(*seconds);
			Logger::Info << std::format("  seed \"{}\" run {}/{}: {:.3f}s\n", seed, run, GEN_TEST_RUNS_PER_SEED,
			                            *seconds);
		}

		if (seedStats.worlds > 0) {
			Detail::LogStats(std::format("  seed \"{}\":", seed), seedStats);
			result.perSeed.emplace_back(seed, seedStats);
		}
	}

	result.completed = ok;
	if (result.overall.worlds > 0)
		Detail::LogStats(ok ? "Chunk gen test overall:" : "Chunk gen test (INCOMPLETE) so far:", result.overall);
	return result;
}

} // namespace ChunkBenchmark