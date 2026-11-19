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

bool FeatureGenerator::GenerateNetherFire(WorldWrapper& _world, Java::Random& _rand, Int3 _pos) {
	for (int i = 0; i < 64; ++i) {
		Int3 testPos{
			_pos.x + _rand.NextInt(8) - _rand.NextInt(8),
			_pos.y + _rand.NextInt(4) - _rand.NextInt(4),
			_pos.z + _rand.NextInt(8) - _rand.NextInt(8),
		};
		// If air with netherrack underneath, generate
		if (_world.GetBlockId(testPos) == BLOCK_AIR && _world.GetBlockId(testPos + Int3{ 0, -1, 0 }) == BLOCK_NETHERRACK)
			_world.SetBlock(testPos, BLOCK_FIRE);
	}
	return true;
}