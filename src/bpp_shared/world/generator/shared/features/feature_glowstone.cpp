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

bool FeatureGenerator::GenerateNetherGlowstone(WorldWrapper& _world, Java::Random& _rand, Int3 _pos) {
	// Exit if tested block isn't air
	if (_world.GetBlockId(_pos) != BLOCK_AIR)
		return false;
	// Exit if block above tested block isn't netherrack
	if (_world.GetBlockId(_pos + Int3{ 0, 1, 0 }) != BLOCK_NETHERRACK)
		return false;
	_world.SetBlock(_pos, BLOCK_GLOWSTONE);
	for (int i = 0; i < 1500; ++i) {
		Int3 testPos{
			_pos.x + _rand.NextInt(8) - _rand.NextInt(8),
			_pos.y - _rand.NextInt(12),
			_pos.z + _rand.NextInt(8) - _rand.NextInt(8),
		};
		// Skip non-air blocks
		if (_world.GetBlockId(testPos) != BLOCK_AIR)
			continue;
		int adjacentGlowstoneCount = 0;
		// Check for adjacent glowstone blocks
		for (int direction = 0; direction < 6; ++direction) {
			BlockType adjacentBlock = BLOCK_AIR;
			switch (direction) {
			case 0:
				adjacentBlock = _world.GetBlockId(testPos + Int3{ -1, 0, 0 });
				break;
			case 1:
				adjacentBlock = _world.GetBlockId(testPos + Int3{ +1, 0, 0 });
				break;
			case 2:
				adjacentBlock = _world.GetBlockId(testPos + Int3{ 0, -1, 0 });
				break;
			case 3:
				adjacentBlock = _world.GetBlockId(testPos + Int3{ 0, +1, 0 });
				break;
			case 4:
				adjacentBlock = _world.GetBlockId(testPos + Int3{ 0, 0, -1 });
				break;
			case 5:
				adjacentBlock = _world.GetBlockId(testPos + Int3{ 0, 0, +1 });
				break;
			default:
				break;
			}
			if (adjacentBlock == BLOCK_GLOWSTONE)
				adjacentGlowstoneCount++;
		}
		// If onle one adjacent glowstone exists, place another
		if (adjacentGlowstoneCount == 1)
			_world.SetBlock(testPos, BLOCK_GLOWSTONE);
	}
	return true;
}