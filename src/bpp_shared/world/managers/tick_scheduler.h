/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once
#include "base_types.h"
#include "blocks.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <numeric_structs.h>
#include <queue>
#include <unordered_set>
#include <utility>
#include <vector>

struct ScheduledTick {
	int64_t tickDue;
	int64_t sequence; // Insertion order
	Int3 pos;
	BlockType expectedBlock;

	bool operator>(const ScheduledTick& _rhs) const {
		if (tickDue != _rhs.tickDue)
			return tickDue > _rhs.tickDue;
		return sequence > _rhs.sequence;
	}
};

struct PendingKey {
	Int3 pos;
	BlockType block;

	bool operator==(const PendingKey& _other) const {
		return pos == _other.pos && block == _other.block;
	}
};

struct PendingKeyHash {
	size_t operator()(const PendingKey& _key) const noexcept {
		size_t h = std::hash<Int3>{}(_key.pos);
		h ^= std::hash<int>{}(int(_key.block)) + 0x9e3779b9u + (h << 6) + (h >> 2);
		return h;
	}
};

class WorldManager;
struct TickScheduler {
	static constexpr size_t MAX_TICKS_PER_TICK = 1000;

	WorldManager* world = nullptr;
	std::priority_queue<ScheduledTick, std::vector<ScheduledTick>, std::greater<ScheduledTick>> scheduledTicks;
	std::unordered_set<PendingKey, PendingKeyHash> pending;

	TickTime currentTick = 0;
	int64_t nextSequence = 0;

	void ScheduleUpdateTick(Int3 _pos, BlockType _block, int _tickDelay) {
		// An equivalent update is already pending for this block
		if (!pending.insert({ _pos, _block }).second)
			return;
		scheduledTicks.push({ currentTick + TickTime(_tickDelay), nextSequence++, _pos, _block });
	}

	void Tick();
};