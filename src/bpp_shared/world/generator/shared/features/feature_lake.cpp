/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../feature_gen.h"
#include "blocks.h"
#include "java_random.h"
#include "numeric_structs.h"
#include "world_wrapper.h"
#include <cstdint>

//  GenerateLake
bool FeatureGenerator::GenerateLake(BlockType _type, WorldWrapper& _world, Java::Random& _rand, Int3 _pos) {
	_pos.x -= 8;
	_pos.z -= 8;

	// Sink to first non-air block
	while (_pos.y > 0 && _world.GetBlockId({ _pos.x, _pos.y, _pos.z }) == BLOCK_AIR)
		--_pos.y;

	_pos.y -= 4;

	bool shapeMask[2048] = {};
	int32_t blobCount = _rand.NextInt(4) + 4;

	for (int32_t blobIndex = 0; blobIndex < blobCount; ++blobIndex) {
		double radX = _rand.NextDouble() * 6.0 + 3.0;
		double radY = _rand.NextDouble() * 4.0 + 2.0;
		double radZ = _rand.NextDouble() * 6.0 + 3.0;
		double cx = _rand.NextDouble() * (16.0 - radX - 2.0) + 1.0 + radX / 2.0;
		double cy = _rand.NextDouble() * (8.0 - radY - 4.0) + 2.0 + radY / 2.0;
		double cz = _rand.NextDouble() * (16.0 - radZ - 2.0) + 1.0 + radZ / 2.0;

		for (int32_t x = 1; x < 15; ++x)
			for (int32_t z = 1; z < 15; ++z)
				for (int32_t y = 1; y < 7; ++y) {
					double dx = (double(x) - cx) / (radX / 2.0);
					double dy = (double(y) - cy) / (radY / 2.0);
					double dz = (double(z) - cz) / (radZ / 2.0);
					if (dx * dx + dy * dy + dz * dz < 1.0)
						shapeMask[(x * 16 + z) * 8 + y] = true;
				}
	}

	// Reject if edges touch existing liquid (above waterline) or non-solid/wrong block (below)
	for (int32_t x = 0; x < 16; ++x)
		for (int32_t z = 0; z < 16; ++z)
			for (int32_t y = 0; y < 8; ++y) {
				bool edge = !shapeMask[(x * 16 + z) * 8 + y] && ((x < 15 && shapeMask[((x + 1) * 16 + z) * 8 + y]) ||
				                                                 (x > 0 && shapeMask[((x - 1) * 16 + z) * 8 + y]) ||
				                                                 (z < 15 && shapeMask[(x * 16 + z + 1) * 8 + y]) ||
				                                                 (z > 0 && shapeMask[(x * 16 + z - 1) * 8 + y]) ||
				                                                 (y < 7 && shapeMask[(x * 16 + z) * 8 + y + 1]) ||
				                                                 (y > 0 && shapeMask[(x * 16 + z) * 8 + y - 1]));
				if (!edge)
					continue;
				BlockType bt = _world.GetBlockId({ _pos.x + x, _pos.y + y, _pos.z + z });
				if (y >= 4 && IsLiquid(bt))
					return false;
				if (y < 4 && !IsSolid(bt) && bt != _type)
					return false;
			}

	// Fill
	for (int32_t x = 0; x < 16; ++x)
		for (int32_t z = 0; z < 16; ++z)
			for (int32_t y = 0; y < 8; ++y)
				if (shapeMask[(x * 16 + z) * 8 + y])
					_world.SetBlock({ _pos.x + x, _pos.y + y, _pos.z + z }, y >= 4 ? BLOCK_AIR : _type);

	// Exposed dirt -> grass
	for (int32_t x = 0; x < 16; ++x)
		for (int32_t z = 0; z < 16; ++z)
			for (int32_t y = 4; y < 8; ++y)
				if (shapeMask[(x * 16 + z) * 8 + y] &&
				    _world.GetBlockId({ _pos.x + x, _pos.y + y - 1, _pos.z + z }) == BLOCK_DIRT &&
				    _world.GetSkyLight({ _pos.x + x, _pos.y + y, _pos.z + z }) > 0)
					_world.SetBlock({ _pos.x + x, _pos.y + y - 1, _pos.z + z }, BLOCK_GRASS);

	// Lava: solidify exposed edges
	if (_type == BLOCK_LAVA_STILL || _type == BLOCK_LAVA_FLOWING) {
		for (int32_t x = 0; x < 16; ++x)
			for (int32_t z = 0; z < 16; ++z)
				for (int32_t y = 0; y < 8; ++y) {
					bool edge = !shapeMask[(x * 16 + z) * 8 + y] && ((x < 15 && shapeMask[((x + 1) * 16 + z) * 8 + y]) ||
					                                                 (x > 0 && shapeMask[((x - 1) * 16 + z) * 8 + y]) ||
					                                                 (z < 15 && shapeMask[(x * 16 + z + 1) * 8 + y]) ||
					                                                 (z > 0 && shapeMask[(x * 16 + z - 1) * 8 + y]) ||
					                                                 (y < 7 && shapeMask[(x * 16 + z) * 8 + y + 1]) ||
					                                                 (y > 0 && shapeMask[(x * 16 + z) * 8 + y - 1]));
					if (edge && (y < 4 || _rand.NextInt(2) != 0) &&
					    IsSolid(_world.GetBlockId({ _pos.x + x, _pos.y + y, _pos.z + z })))
						_world.SetBlock({ _pos.x + x, _pos.y + y, _pos.z + z }, BLOCK_STONE);
				}
	}
	return true;
}