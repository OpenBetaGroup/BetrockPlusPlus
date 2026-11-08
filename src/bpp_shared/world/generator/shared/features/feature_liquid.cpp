/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../feature_gen.h"

//  Attempts to generate a singular liquid source block
bool FeatureGenerator::GenerateLiquid(BlockType _type, WorldWrapper& _world, Java::Random& /*_rand*/, Int3 _pos,
                                      bool _nether) {
	const BlockType solidBlock = _nether ? BLOCK_NETHERRACK : BLOCK_STONE;
	if (_world.GetBlockId({ _pos.x, _pos.y + 1, _pos.z }) != solidBlock)
		return false;
	if (_world.GetBlockId({ _pos.x, _pos.y - 1, _pos.z }) != solidBlock)
		return false;
	const BlockType cur = _world.GetBlockId(_pos);
	if (cur != BLOCK_AIR && cur != solidBlock)
		return false;

	int32_t solid = 0, air = 0;
	if (_world.GetBlockId({ _pos.x - 1, _pos.y, _pos.z }) == solidBlock)
		++solid;
	if (_world.GetBlockId({ _pos.x + 1, _pos.y, _pos.z }) == solidBlock)
		++solid;
	if (_world.GetBlockId({ _pos.x, _pos.y, _pos.z - 1 }) == solidBlock)
		++solid;
	if (_world.GetBlockId({ _pos.x, _pos.y, _pos.z + 1 }) == solidBlock)
		++solid;
	if (_world.GetBlockId({ _pos.x - 1, _pos.y, _pos.z }) == BLOCK_AIR)
		++air;
	if (_world.GetBlockId({ _pos.x + 1, _pos.y, _pos.z }) == BLOCK_AIR)
		++air;
	if (_world.GetBlockId({ _pos.x, _pos.y, _pos.z - 1 }) == BLOCK_AIR)
		++air;
	if (_world.GetBlockId({ _pos.x, _pos.y, _pos.z + 1 }) == BLOCK_AIR)
		++air;

	if (solid == 3 && air == 1) {
		_world.SetBlock(_pos, _type);
	}
	return true;
}