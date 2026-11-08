/*
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */
#pragma once

#include "base_types.h"
#include "dimensions.h"
#include "entities.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <unordered_map>

struct TickEntityKey {
	Dimension dimension;
	EntityId id;
	EntityType type;
	bool operator==(const TickEntityKey&) const = default;
};

struct TickEntityKeyHash {
	size_t operator()(const TickEntityKey& key) const noexcept {
		size_t hash = std::hash<int>{}(static_cast<int>(key.dimension));
		hash ^= std::hash<int32_t>{}(static_cast<int32_t>(key.id)) + size_t{ 0x9e3779b9 } + (hash << 6) + (hash >> 2);
		hash ^= std::hash<int>{}(static_cast<int>(key.type)) + size_t{ 0x9e3779b9 } + (hash << 6) + (hash >> 2);
		return hash;
	}
};

enum class TickTask : uint8_t {
	TotalTick,
	Network,
	Autosave,
	OffTickTasks,
	MobSpawning,
	ChunkLoading,
	ChunkUnloading,
	BlockUpdates,
	EntityTicks,
	BlockEntityTicks,
	Environment,
	Count
};

class TickProfiler {
public:
	struct EntitySample {
		std::chrono::nanoseconds elapsed{ 0 };
		uint64_t count = 0;
	};

	void Record(TickTask task, std::chrono::nanoseconds elapsed) noexcept {
		totals[static_cast<size_t>(task)] += elapsed;
	}
	void EnableEntityProfiling(bool enabled) noexcept {
		entityProfilingEnabled = enabled;
	}
	bool EntityProfilingEnabled() const noexcept {
		return entityProfilingEnabled;
	}

	void RecordEntity(Dimension dimension, EntityId id, EntityType type, std::chrono::nanoseconds elapsed) {
		if (!entityProfilingEnabled)
			return;
		auto& sample = entities[{ dimension, id, type }];
		sample.elapsed += elapsed;
		++sample.count;
	}

	void Reset() {
		totals.fill(std::chrono::nanoseconds{ 0 });
		entities.clear();
		entityProfilingEnabled = false;
	}
	std::chrono::nanoseconds Get(TickTask task) const noexcept {
		return totals[static_cast<size_t>(task)];
	}
	const auto& EntitySamples() const noexcept {
		return entities;
	}

private:
	std::array<std::chrono::nanoseconds, static_cast<size_t>(TickTask::Count)> totals{};
	std::unordered_map<TickEntityKey, EntitySample, TickEntityKeyHash> entities;
	bool entityProfilingEnabled = false;
};
