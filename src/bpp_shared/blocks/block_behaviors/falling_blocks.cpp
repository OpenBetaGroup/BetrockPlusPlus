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

void RegisterFallingBlockBehaviors() {
	// Falling blocks!
	blockBehaviors[BLOCK_GRAVEL].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos,
	                                                        BlockType /*_blockId*/) -> void {
		// Schedule a check to see if we can fall
		_world.tickScheduler.ScheduleUpdateTick(_pos, BLOCK_GRAVEL, 3);
	};
	blockBehaviors[BLOCK_GRAVEL].onBlockAdded = [](WorldManager& _world, Int3 _pos) -> void {
		_world.tickScheduler.ScheduleUpdateTick(_pos, BLOCK_GRAVEL, 3);
	};
	blockBehaviors[BLOCK_GRAVEL].onTick = [](WorldManager& _world, Int3 _pos, uint8_t /*_meta*/,
	                                         Java::Random& /*_random*/) -> void {
		const Int3 below = _pos.WithOffset(Cardinal::Value::Down);

		if (!Blocks::CanFallAt(_world, below) || _pos.y < 0)
			return;

		constexpr int32_t CHECK_RADIUS = 32; // Blocks
		bool areaLoaded = _world.AABBinValidChunks({ double(_pos.x - CHECK_RADIUS), double(_pos.y),
		                                             double(_pos.z - CHECK_RADIUS), double(_pos.x + CHECK_RADIUS),
		                                             double(_pos.y), double(_pos.z + CHECK_RADIUS) });

		if (areaLoaded) {
			Vec3 spawnPos = { _pos.x + 0.5, _pos.y + 0.5, _pos.z + 0.5 };
			auto entity = std::make_shared<FallingBlockEntity>(spawnPos, BLOCK_GRAVEL);
			_world.entityManager.AddEntity(std::move(entity));
		} else {
			_world.SetBlock(_pos, BLOCK_AIR, 0);

			Int3 landing = _pos;
			while (Blocks::CanFallAt(_world, landing.WithOffset(Cardinal::Value::Down)) && landing.y > 0)
				landing.y--;

			if (landing.y > 0)
				_world.SetBlock(landing, BLOCK_GRAVEL, 0);
		}
	};
	blockBehaviors[BLOCK_SAND].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos,
	                                                      BlockType /*_blockId*/) -> void {
		// Schedule a check to see if we can fall
		_world.tickScheduler.ScheduleUpdateTick(_pos, BLOCK_SAND, 3);
	};
	blockBehaviors[BLOCK_SAND].onBlockAdded = [](WorldManager& _world, Int3 _pos) -> void {
		_world.tickScheduler.ScheduleUpdateTick(_pos, BLOCK_SAND, 3);
	};
	blockBehaviors[BLOCK_SAND].onTick = [](WorldManager& _world, Int3 _pos, uint8_t /*_meta*/,
	                                       Java::Random& /*_random*/) -> void {
		Int3 below = { _pos.x, _pos.y - 1, _pos.z };

		if (!Blocks::CanFallAt(_world, below) || _pos.y < 0)
			return;

		constexpr int32_t CHECK_RADIUS = 32; // Blocks
		bool areaLoaded = _world.AABBinValidChunks({ double(_pos.x - CHECK_RADIUS), double(_pos.y),
		                                             double(_pos.z - CHECK_RADIUS), double(_pos.x + CHECK_RADIUS),
		                                             double(_pos.y), double(_pos.z + CHECK_RADIUS) });

		if (areaLoaded) {
			Vec3 spawnPos = { _pos.x + 0.5, _pos.y + 0.5, _pos.z + 0.5 };
			auto entity = std::make_shared<FallingBlockEntity>(spawnPos, BLOCK_SAND);
			_world.entityManager.AddEntity(std::move(entity));
		} else {
			_world.SetBlock(_pos, BLOCK_AIR, 0);

			Int3 landing = _pos;
			while (Blocks::CanFallAt(_world, { landing.x, landing.y - 1, landing.z }) && landing.y > 0)
				landing.y--;

			if (landing.y > 0)
				_world.SetBlock(landing, BLOCK_SAND, 0);
		}
	};
}

}; // namespace Blocks