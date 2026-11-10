/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#pragma once

#include "world/world_wrapper.h"
#include "blocks/block_properties.h"
#include "constants.h"
#include "java_random.h"

// Inline block-property helpers
inline constexpr bool IsSolid(BlockType _t) {
	return Blocks::blockProperties[_t].material.isSolid;
}
inline constexpr bool IsLiquid(BlockType _t) {
	return Blocks::blockProperties[_t].material.isLiquid;
}
inline constexpr bool IsOpaque(BlockType _t) {
	return Blocks::blockProperties[_t].lightOpacity > 0;
}

// Used for generating features in the world
namespace FeatureGenerator {
// Overworld features
bool GenerateLake(BlockType _type, WorldWrapper& _world, Java::Random& _rand, Int3 _pos);
bool GenerateDungeon(WorldWrapper& _world, Java::Random& _rand, Int3 _pos);
bool GenerateClay(WorldWrapper& _world, Java::Random& _rand, Int3 _pos, int32_t _blobSize = 0);
bool GenerateOre(BlockType _type, WorldWrapper& _world, Java::Random& _rand, Int3 _pos, int32_t _blobSize = 0);
bool GenerateFlowers(BlockType _type, WorldWrapper& _world, Java::Random& _rand, Int3 _pos);
bool GenerateTallgrass(uint8_t _meta, WorldWrapper& _world, Java::Random& _rand, Int3 _pos);
bool GenerateDeadbush(WorldWrapper& _world, Java::Random& _rand, Int3 _pos);
bool GenerateSugarcane(WorldWrapper& _world, Java::Random& _rand, Int3 _pos);
bool GeneratePumpkins(WorldWrapper& _world, Java::Random& _rand, Int3 _pos);
bool GenerateCacti(WorldWrapper& _world, Java::Random& _rand, Int3 _pos);
bool GenerateLiquid(BlockType _type, WorldWrapper& _world, Java::Random& _rand, Int3 _pos, bool _nether);
// Nether Features
bool GenerateNetherFire(WorldWrapper& _world, Java::Random& _rand, Int3 _pos);
bool GenerateNetherGlowstone(WorldWrapper& _world, Java::Random& _rand, Int3 _pos);

ItemStack GenerateDungeonChestLoot(Java::Random& _rand);
std::string PickMobToSpawn(Java::Random& _rand);
}; // namespace FeatureGenerator