/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

// Lighter used by the main world.
#pragma once
#include "blocks/block_properties.h"
#include "chunk.h"
#include <climits>
#include <cstdint>
#include <numeric_structs.h>
#include <vector>

class WorldManager;

enum class LightType : uint8_t {
	Sky,
	Block
};

// A light update entry covering a 3-D axis-aligned region [min, max] inclusive.
struct LightRegion {
	Int3 min{ 0, 0, 0 };
	Int3 max{ 0, 0, 0 };
	LightType type;

	// Returns true if [x1,y1,z1]->[x2,y2,z2] is contained within (or close
	// enough to merge into) this region, expanding it in-place when the volume
	// grows by <=2
	bool TryMerge(int _x1, int _y1, int _z1, int _x2, int _y2, int _z2) {
		if (_x1 >= min.x && _y1 >= min.y && _z1 >= min.z && _x2 <= max.x && _y2 <= max.y && _z2 <= max.z)
			return true;

		if (_x1 < min.x - 1 || _y1 < min.y - 1 || _z1 < min.z - 1)
			return false;
		if (_x2 > max.x + 1 || _y2 > max.y + 1 || _z2 > max.z + 1)
			return false;

		int oldVol = (max.x - min.x + 1) * (max.y - min.y + 1) * (max.z - min.z + 1);
		int nx1 = std::min<int>(min.x, _x1), ny1 = std::min<int>(min.y, _y1),
		    nz1 = std::min<int>(min.z, _z1);
		int nx2 = std::max<int>(max.x, _x2), ny2 = std::max<int>(max.y, _y2),
		    nz2 = std::max<int>(max.z, _z2);
		int newVol = (nx2 - nx1 + 1) * (ny2 - ny1 + 1) * (nz2 - nz1 + 1);
		if (newVol - oldVol > 2)
			return false;

		min = { nx1, ny1, nz1 };
		max = { nx2, ny2, nz2 };
		return true;
	}
};

struct UnlightUpdate {
	Int3 pos{ 0, 0, 0 };
	LightType type;
	int val;
};

// 3x3 grid of chunk pointers centered on a chunk coordinate.
struct ChunkCache {
	Chunk* grid[3][3] = {};
	int cx = INT_MIN, cz = INT_MIN; // chunk coords of center (grid[1][1])

	// Fetch chunk at chunk-coord (tcx, tcz) from the grid.
	Chunk* Get(int _tcx, int _tcz) const {
		int dx = _tcx - cx, dz = _tcz - cz;
		if (dx < -1 || dx > 1 || dz < -1 || dz > 1)
			return nullptr;
		return grid[dx + 1][dz + 1];
	}

	// Refresh the entire 3x3 grid around (ncx, ncz) if the center has changed.
	void Refresh(int _ncx, int _ncz, WorldManager& _world);
};

struct Lighter {
public:
	static constexpr size_t MAX_LIGHT_QUEUE = 1000000;

	Lighter() {
		// Avoid repeated vector growth/reallocation during propagation bursts
		// (explosions, mass sky-column removal, etc). Small enough to not
		// matter at steady-state, big enough to absorb most normal bursts
		// before the vector has to double.
		lightQueue.reserve(4096);
		unlightQueue.reserve(256);
	}

	void PropagateLightAt(int _x, int _y, int _z, LightType _type, WorldManager& _world, ChunkCache& _cache);
	void UnlightAt(int _x, int _y, int _z, LightType _type, WorldManager& _world);

	// Process up to `maxIterations` light-queue entries this call.
	bool ProcessLightQueue(WorldManager& _world, int _maxIterations = INT_MAX);

	// Schedule a single-block update; bypasses merge (used for BFS fan-out).
	void ScheduleLightUpdate(Int3 _pos, LightType _type) {
		if (_pos.y < 0 || _pos.y >= CHUNK_HEIGHT)
			return;
		if (lightQueue.size() < MAX_LIGHT_QUEUE)
			lightQueue.push_back({ _pos, _pos, _type });
	}

	// Schedule a region [mn, mx] update with merge/dedup
	void ScheduleLightRegion(Int3 _mn, Int3 _mx, LightType _type) {
		_mn.y = std::max<int>(_mn.y, 0);
		_mx.y = std::min<int>(_mx.y, CHUNK_HEIGHT - 1);
		if (_mn.y > _mx.y)
			return;

		size_t checkCount = std::min<int>(lightQueue.size(), size_t(5));
		for (size_t i = 0; i < checkCount; ++i) {
			LightRegion& r = lightQueue[size_t(lightQueue.size() - 1 - i)];
			if (r.type != _type)
				continue;
			if (r.TryMerge(_mn.x, _mn.y, _mn.z, _mx.x, _mx.y, _mx.z))
				return;
		}

		if (lightQueue.size() >= MAX_LIGHT_QUEUE)
			return;
		lightQueue.push_back({ _mn, _mx, _type });
	}

private:
	std::vector<LightRegion> lightQueue;
	std::vector<UnlightUpdate> unlightQueue;
	int processingDepth = 0;
};

namespace Lighting {
extern const float BrightnessArray[16];
} // namespace Lighting