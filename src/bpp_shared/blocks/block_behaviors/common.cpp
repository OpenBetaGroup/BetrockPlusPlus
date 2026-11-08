/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "blocks.h"
#include "blocks/block_behaviors.h"
#include "blocks/block_properties.h"
#include "dimensions.h"
#include "entities/entity_falling_block.h"
#include "entities/entity_player.h"
#include "entities/entity_skeleton.h"
#include "entities/entity_spider.h"
#include "entities/entity_zombie.h"
#include "enums/items.h"
#include "generator/overworld/tree_gen.h"
#include "helpers/direction_fixer.h"
#include "helpers/java/java_math.h"
#include "internal.h"
#include "items/item_properties.h"
#include "logger.h"
#include "managers/rail_manager.h"
#include "managers/redstone_manager.h"
#include "managers/tick_scheduler.h"
#include "numeric_structs.h"
#include "packet_data.h"
#include "tile_entities/tile_entity.h"
#include "world.h"

namespace Blocks {

bool IsReplaceable(WorldManager& _world, Int3 _pos) {
	BlockType existing = _world.GetBlockId(_pos);
	return existing == BLOCK_AIR || existing == BLOCK_WATER_FLOWING || existing == BLOCK_WATER_STILL ||
	       existing == BLOCK_LAVA_FLOWING || existing == BLOCK_LAVA_STILL || existing == BLOCK_FIRE ||
	       existing == BLOCK_SNOW_LAYER;
};

bool IsSupported(WorldManager& _world, Int3 _pos, Cardinal::Value _dir) {
	Int3 support = _pos.WithOffset(Cardinal::Opposite(_dir));
	return _world.IsBlockNormalCube(support);
};

void GenericExplode(WorldManager& _world, Int3 _pos) {
	BreakAndDropBlockWithChance(_world, _pos, 0.3f);
}

void GenericBreak(WorldManager& _world, Int3 _pos, Entity& /*_destroyer*/) {
	BreakAndDropBlock(_world, _pos);
}

// Returns the position the block could be placed at; (-1, -1, -1) if invalid
Int3 CanPlace(WorldManager& _world, Int3 _pos, Entity& /*_placer*/, Cardinal::Value _face, BlockType _blockId,
              uint8_t _meta) {
	Int3 invalid = { -1, -1, -1 };
	if (!_world.InBounds(_pos.y))
		return invalid;

	BlockType existing = _world.GetBlockId(_pos);
	Int3 sourceBlock = _pos.WithOffset(Cardinal::Opposite(_face));

	// Check if we can replace snow
	Int3 targetPos = _pos;
	if (_world.GetBlockId(sourceBlock) == BLOCK_SNOW_LAYER) {
		targetPos = sourceBlock;
	} else {
		// Check if this block is replaceable
		// Should match vanilla?
		bool replaceable = existing == BLOCK_AIR || existing == BLOCK_WATER_FLOWING || existing == BLOCK_WATER_STILL ||
		                   existing == BLOCK_LAVA_FLOWING || existing == BLOCK_LAVA_STILL || existing == BLOCK_FIRE ||
		                   existing == BLOCK_SNOW_LAYER;
		if (!replaceable)
			return invalid;
	}

	// Check to see if any entities overlap our block's collider
	if (Blocks::blockProperties[_blockId].isCollidable) {
		auto blockCollider = Blocks::blockBehaviors[_blockId].getCollider(_meta).Offset(
		    targetPos.x, targetPos.y,
		    targetPos.z); // Block colliders are at the origin so shift to world space
		auto entitiesInBlock = _world.entityManager.GetEntitiesWithinAabb(
		    { double(targetPos.x), double(targetPos.y), double(targetPos.z), double(targetPos.x) + 1.0,
		      double(targetPos.y) + 1.0, double(targetPos.z) + 1.0 });
		for (auto& entity : entitiesInBlock) {
			if (blockCollider.Intersects(entity->collider) && entity->preventEntitySpawning) {
				return invalid;
			}
		}
	}

	return targetPos;
}

bool GenericPlace(WorldManager& _world, Int3 _pos, Entity& _placer, Cardinal::Value _face, BlockType _blockId,
                  uint8_t _meta) {
	Int3 placePos = CanPlace(_world, _pos, _placer, _face, _blockId, _meta);

	if (placePos == Int3{ -1, -1, -1 })
		return false;

	if (Blocks::blockProperties[_blockId].material.isSolid && placePos.y >= CHUNK_HEIGHT - 1)
		return false;

	_world.SetBlock(placePos, _blockId, _meta);
	return true;
}

}; // namespace Blocks
