/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#include "lighter.h"
#include "blocks.h"
#include "constants.h"
#include "world.h"
#include <cassert>
#include <cstring>

namespace Lighting {
const float BrightnessArray[16] = { 0.035f, 0.044f, 0.055f, 0.069f, 0.086f, 0.107f, 0.134f, 0.168f,
	                                0.21f,  0.262f, 0.328f, 0.41f,  0.512f, 0.64f,  0.8f,   1.0f };
} // namespace Lighting

// ChunkCache so we don't have to do map lookups for every neighbor access during light propagation.
void ChunkCache::Refresh(int _ncx, int _ncz, WorldManager& _world) {
	if (_ncx == cx && _ncz == cz)
		return;
	cx = _ncx;
	cz = _ncz;
	for (int dx = -1; dx <= 1; ++dx) {
		for (int dz = -1; dz <= 1; ++dz) {
			int tcx = _ncx + dx, tcz = _ncz + dz;
			Chunk* fetched = _world.GetChunkRaw({ tcx, tcz });
			Chunk* c = (fetched && fetched->state.load(std::memory_order_acquire) >= ChunkState::Generated) ? fetched
			                                                                                                : nullptr;
			grid[dx + 1][dz + 1] = c;
		}
	}
}

static inline int GetLightDirect(Chunk* _chunk, int _lx, int _y, int _lz, LightType _type) {
	if (!_chunk || _y < 0 || _y >= CHUNK_HEIGHT)
		return 0;
	return (_type == LightType::Sky) ? _chunk->GetSkyLight({ _lx, _y, _lz }) : _chunk->GetBlockLight({ _lx, _y, _lz });
}

void Lighter::PropagateLightAt(int _x, int _y, int _z, LightType _type, WorldManager& _world, ChunkCache& _cache) {
	if (_y < 0 || _y >= CHUNK_HEIGHT)
		return;

	int cx = _x >> 4, cz = _z >> 4;

	// Refresh the 3x3 cache only when we cross a chunk boundary.
	_cache.Refresh(cx, cz, _world);

	Chunk* chunk = _cache.grid[1][1]; // Center
	if (!chunk)
		return;

	int lx = _x & 15, lz = _z & 15;
	BlockType blockId = chunk->GetBlock({ lx, _y, lz });
	int opacity = Blocks::blockProperties[blockId].lightOpacity;
	if (opacity == 0)
		opacity = 1;

	// Pick neighbor chunk pointers
	Chunk* cxn = (lx == 0) ? _cache.grid[0][1] : chunk;
	Chunk* cxp = (lx == 15) ? _cache.grid[2][1] : chunk;
	Chunk* czn = (lz == 0) ? _cache.grid[1][0] : chunk;
	Chunk* czp = (lz == 15) ? _cache.grid[1][2] : chunk;

	int lxn = (lx == 0) ? 15 : lx - 1;
	int lxp = (lx == 15) ? 0 : lx + 1;
	int lzn = (lz == 0) ? 15 : lz - 1;
	int lzp = (lz == 15) ? 0 : lz + 1;

	// Cache each neighbor's light value the first time it's read so the later
	// propagation check below doesn't have to re-query the same cell.
	// Order: -x, +x, -y, +y, -z, +z
	int neighborLight[6];
	bool haveNeighbor[6] = { false, false, false, false, false, false };

	auto neighbor = [&](int _i, Chunk* _nc, int _nlx, int _ny, int _nlz) -> int {
		if (!haveNeighbor[_i]) {
			neighborLight[_i] = GetLightDirect(_nc, _nlx, _ny, _nlz, _type);
			haveNeighbor[_i] = true;
		}
		return neighborLight[_i];
	};

	auto scanBest = [&]() -> int {
		int best = 0;
		best = std::max<int>(best, neighbor(0, cxn, lxn, _y, lz));
		best = std::max<int>(best, neighbor(1, cxp, lxp, _y, lz));
		best = std::max<int>(best, neighbor(2, chunk, lx, _y - 1, lz));
		best = std::max<int>(best, neighbor(3, chunk, lx, _y + 1, lz));
		best = std::max<int>(best, neighbor(4, czn, lx, _y, lzn));
		best = std::max<int>(best, neighbor(5, czp, lx, _y, lzp));
		return best;
	};

	int newVal = 0;

	if (_type == LightType::Sky) {
		if (chunk->CanBlockSeeSky({ lx, _y, lz })) {
			newVal = 15;
		} else if (opacity < 15) {
			newVal = std::max<int>(0, scanBest() - opacity);
		}
		int oldVal = chunk->GetSkyLight({ lx, _y, lz });
		if (oldVal == newVal)
			return;
		chunk->SetSkyLight({ lx, _y, lz }, uint8_t(newVal));
		// Call a block update on the block that had its lighting updated
		// Beta doesn't have a direct on lighting change packet for server -> client
		// So this is what we have to resort to
		// Technically, this doesn't work for any light update that doesn't change more than 9 blocks but its good enough
		if (_world.onBlockUpdate)
			_world.onBlockUpdate(PendingBlock{ .block{ BlockType(blockId), chunk->GetMeta({ lx, _y, lz }) },
			                                   .blockPos{ _x, _y, _z },
			                                   .light{ chunk->GetBlockLight({ lx, _y, lz }), uint8_t(newVal) } },
			                     chunk->cpos);
	} else {
		int emitted = Blocks::blockProperties[blockId].lightEmission;
		if (opacity < 15) {
			newVal = std::max<int>(emitted, scanBest() - opacity);
		} else {
			newVal = emitted;
		}
		int oldVal = chunk->GetBlockLight({ lx, _y, lz });
		if (oldVal == newVal)
			return;
		chunk->SetBlockLight({ lx, _y, lz }, uint8_t(newVal));
		if (_world.onBlockUpdate)
			_world.onBlockUpdate(
			    PendingBlock{
			        .block{ BlockType(blockId), chunk->GetMeta({ lx, _y, lz }) },
			        .blockPos{ _x, _y, _z },
			        .light{ uint8_t(newVal), chunk->GetSkyLight({ lx, _y, lz }) },
			    },
			    chunk->cpos);
	}

	// Propagate to neighbors. Reuses the cached reads from scanBest() above
	// when it ran (the common opacity < 15 case), so most neighbors here cost
	// zero extra chunk-array lookups instead of a second GetLightDirect call.
	auto maybeQueue = [&](int _nx, int _ny, int _nz, Chunk* _nc, int _nlx, int _nlz, int _i) {
		if (_ny < 0 || _ny >= CHUNK_HEIGHT || !_nc)
			return;
		int expected = std::max<int>(0, newVal - 1);
		if (neighbor(_i, _nc, _nlx, _ny, _nlz) < expected && lightQueue.size() < MAX_LIGHT_QUEUE)
			lightQueue.push_back({ { _nx, _ny, _nz }, { _nx, _ny, _nz }, _type });
	};

	maybeQueue(_x - 1, _y, _z, cxn, lxn, lz, 0);
	maybeQueue(_x + 1, _y, _z, cxp, lxp, lz, 1);
	maybeQueue(_x, _y - 1, _z, chunk, lx, lz, 2);
	maybeQueue(_x, _y + 1, _z, chunk, lx, lz, 3);
	maybeQueue(_x, _y, _z - 1, czn, lx, lzn, 4);
	maybeQueue(_x, _y, _z + 1, czp, lx, lzp, 5);
}

void Lighter::UnlightAt(int _x, int _y, int _z, LightType _type, WorldManager& _world) {
	// Separate function for this since beta's lighting engine can't decrease light values in place
	if (_y < 0 || _y >= CHUNK_HEIGHT)
		return;

	ChunkCache unlightCache;

	unlightCache.Refresh(_x >> 4, _z >> 4, _world);
	Chunk* chunk = unlightCache.grid[1][1];
	if (!chunk)
		return;

	int lx = _x & 15, lz = _z & 15;
	int oldVal = (_type == LightType::Sky) ? chunk->GetSkyLight({ lx, _y, lz }) : chunk->GetBlockLight({ lx, _y, lz });
	if (oldVal == 0)
		return;

	// Block/meta and the untouched light channel aren't affected by the light
	// write below, so grab them once now instead of re-reading after.
	BlockType blockId = BlockType(chunk->GetBlock({ lx, _y, lz }));
	uint8_t meta = chunk->GetMeta({ lx, _y, lz });
	uint8_t otherLight = (_type == LightType::Sky) ? chunk->GetBlockLight({ lx, _y, lz })
	                                               : chunk->GetSkyLight({ lx, _y, lz });

	if (_type == LightType::Sky)
		chunk->SetSkyLight({ lx, _y, lz }, 0);
	else
		chunk->SetBlockLight({ lx, _y, lz }, 0);
	if (_world.onBlockUpdate)
		_world.onBlockUpdate(
		    PendingBlock{
		        .block{ blockId, meta },
		        .blockPos{ _x, _y, _z },
		        // Whichever type we just unlit was just set to 0 above
		        .light{ (_type == LightType::Block) ? uint8_t(0) : otherLight,
		                (_type == LightType::Sky) ? uint8_t(0) : otherLight },
		    },
		    chunk->cpos);

	unlightQueue.push_back({ { _x, _y, _z }, _type, oldVal });

	// Drain everything at once since there isn't a lot of unlighting events
	while (!unlightQueue.empty()) {
		auto [pos, t, val] = unlightQueue.back();
		unlightQueue.pop_back();

		unlightCache.Refresh(pos.x >> 4, pos.z >> 4, _world);

		const int ndx[] = { -1, 1, 0, 0, 0, 0 };
		const int ndy[] = { 0, 0, -1, 1, 0, 0 };
		const int ndz[] = { 0, 0, 0, 0, -1, 1 };
		for (int i = 0; i < 6; ++i) {
			int nx = pos.x + ndx[i];
			int ny = pos.y + ndy[i];
			int nz = pos.z + ndz[i];
			if (ny < 0 || ny >= CHUNK_HEIGHT)
				continue;

			int ncx = nx >> 4, ncz = nz >> 4;
			int nlx = nx & 15, nlz = nz & 15;
			int dx = ncx - unlightCache.cx, dz = ncz - unlightCache.cz;
			// unlightCache was just refreshed to pos's chunk, and (nx,ny,nz) is
			// only one block away from pos, so the neighbor chunk is always
			// within the refreshed 3x3 grid; no map-lookup fallback needed.
			assert(dx >= -1 && dx <= 1 && dz >= -1 && dz <= 1);
			Chunk* nc = unlightCache.grid[dx + 1][dz + 1];
			if (!nc)
				continue;

			int nVal = (t == LightType::Sky) ? nc->GetSkyLight({ nlx, ny, nlz }) : nc->GetBlockLight({ nlx, ny, nlz });
			if (nVal == 0)
				continue;

			if (nVal < val) {
				// Block/meta aren't affected by the light write; grab them first.
				BlockType nBlockId = BlockType(nc->GetBlock({ nlx, ny, nlz }));
				uint8_t nMeta = nc->GetMeta({ nlx, ny, nlz });
				uint8_t nOtherLight = (t == LightType::Sky) ? nc->GetBlockLight({ nlx, ny, nlz })
				                                            : nc->GetSkyLight({ nlx, ny, nlz });

				if (t == LightType::Sky)
					nc->SetSkyLight({ nlx, ny, nlz }, 0);
				else
					nc->SetBlockLight({ nlx, ny, nlz }, 0);
				if (_world.onBlockUpdate)
					_world.onBlockUpdate(
					    PendingBlock{
					        .block{ nBlockId, nMeta },
					        .blockPos{ nx, ny, nz },
					        .light{ (t == LightType::Block) ? uint8_t(0) : nOtherLight,
					                (t == LightType::Sky) ? uint8_t(0) : nOtherLight },
					    },
					    nc->cpos);
				unlightQueue.push_back({ { nx, ny, nz }, t, nVal });
				// Always re-queue for re-light
				ScheduleLightUpdate({ nx, ny, nz }, t);
			} else {
				// Neighbor is at least as bright
				ScheduleLightUpdate({ nx, ny, nz }, t);
			}
		}
	}
}

bool Lighter::ProcessLightQueue(WorldManager& _world, int _maxIterations) {
	if (processingDepth >= 50)
		return !lightQueue.empty();
	++processingDepth;

	ChunkCache cache;
	int iters = 0;

	while (!lightQueue.empty() && iters < _maxIterations) {
		LightRegion region = lightQueue.back();
		lightQueue.pop_back();
		++iters;

		int dx = region.max.x - region.min.x + 1;
		int dy = region.max.y - region.min.y + 1;
		int dz = region.max.z - region.min.z + 1;
		if (dx * dy * dz > 32768)
			continue;

		region.min.y = std::max<int>(region.min.y, 0);
		region.max.y = std::min<int>(region.max.y, CHUNK_HEIGHT - 1);

		for (int x = region.min.x; x <= region.max.x; ++x)
			for (int z = region.min.z; z <= region.max.z; ++z)
				for (int y = region.min.y; y <= region.max.y; ++y)
					PropagateLightAt(x, y, z, region.type, _world, cache);
	}

	--processingDepth;
	if (lightQueue.empty())
		lightQueue.shrink_to_fit();

	return !lightQueue.empty();
}