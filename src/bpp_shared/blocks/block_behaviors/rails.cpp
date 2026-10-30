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
#include "tile_entities/tile_entity.h"
#include "world.h"

namespace Blocks {

static void SetDetectorRailState(WorldManager& _world, Int3 _pos, uint8_t _meta) {
	bool powered = (_meta & 8) != 0;
	bool detectedMinecart = false;
	double inset = 0.125;
	AABB detectorBox = {
		_pos.x + inset, static_cast<double>(_pos.y), _pos.z + inset,
		_pos.x + 1 - inset, _pos.y + 0.25, _pos.z + 1 - inset
	};
	EntityType type = EntityType::MINECART;
	auto minecarts = _world.entityManager.GetEntitiesWithinAabbOfType(detectorBox, type);
	detectedMinecart = minecarts.size() > 0;

	if (detectedMinecart && !powered) {
		_world.SetMeta(_pos, _meta | 8);
		_world.NotifyNeighborsOfUpdate(_pos, BLOCK_RAIL_DETECTOR);
		_world.NotifyNeighborsOfUpdate(_pos.WithOffset(Cardinal::Value::Down), BLOCK_RAIL_DETECTOR);
	}

	if (!detectedMinecart && powered) {
		_world.SetMeta(_pos, _meta & 7);
		_world.NotifyNeighborsOfUpdate(_pos, BLOCK_RAIL_DETECTOR);
		_world.NotifyNeighborsOfUpdate(_pos.WithOffset(Cardinal::Value::Down), BLOCK_RAIL_DETECTOR);
	}

	if (detectedMinecart)
		_world.tickScheduler.ScheduleUpdateTick(_pos, BLOCK_RAIL_DETECTOR, 20);
}

static void UpdateRail(WorldManager& _world, Int3 _pos, BlockType _block) {
	RailManager::RefreshRail(_world, _pos, _block);
}

static bool CanRailStay(WorldManager& _world, Int3 _pos, BlockType _block) {
	if (!_world.IsBlockNormalCube(_pos.WithOffset(Cardinal::Value::Down)))
		return false;

	auto shape = RailManager::GetRailShape(_world.GetMetadata(_pos), _block);
	switch (shape) {
	case Blocks::RailShape::AscendingEast:
		return _world.IsBlockNormalCube(_pos.WithOffset(Cardinal::Value::East));
	case Blocks::RailShape::AscendingWest:
		return _world.IsBlockNormalCube(_pos.WithOffset(Cardinal::Value::West));
	case Blocks::RailShape::AscendingNorth:
		return _world.IsBlockNormalCube(_pos.WithOffset(Cardinal::Value::North));
	case Blocks::RailShape::AscendingSouth:
		return _world.IsBlockNormalCube(_pos.WithOffset(Cardinal::Value::South));
	default:
		return true;
	}
}

void RegisterRailBehaviors() {
	// Rails
	blockBehaviors[BlockType::BLOCK_RAIL] = {
		.getRayBounds = RailAabb,
		.getCollider = EmptyCollider,
	};
	blockBehaviors[BlockType::BLOCK_RAIL_POWERED] = {
		.getRayBounds = RailAabb,
		.getCollider = EmptyCollider,
	};
	blockBehaviors[BlockType::BLOCK_RAIL_DETECTOR] = {
		.getRayBounds = RailAabb,
		.getCollider = EmptyCollider,
	};

	// Rail
	blockBehaviors[BLOCK_RAIL].onBlockPlaced = [](WorldManager& _world, Int3 _pos, Entity& _placer,
	                                              Cardinal::Value _face, BlockType _blockId, uint8_t _meta) -> bool {
		auto newPos = CanPlace(_world, _pos, _placer, _face, _blockId, _meta);
		if (newPos == Int3{ -1, -1, -1 })
			return false;
		if (!CanRailStay(_world, newPos, BLOCK_RAIL))
			return false;
		return GenericPlace(_world, _pos, _placer, _face, _blockId, _meta);
	};
	blockBehaviors[BLOCK_RAIL].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos, BlockType _blockId) -> void {
		if (!CanRailStay(_world, _pos, BLOCK_RAIL)) {
			BreakAndDropBlock(_world, _pos);
			return;
		}
		if (RedstoneManager::CanProvidePower(_blockId) && RailManager::GetAdjacentTrackCount(_world, _pos) == 3) {
			RailManager::RefreshRail(_world, _pos, BLOCK_RAIL, /*_forceWrite=*/false);
		}
	};
	blockBehaviors[BLOCK_RAIL].onBlockAdded = [](WorldManager& _world, Int3 _pos) -> void {
		UpdateRail(_world, _pos, BLOCK_RAIL);
	};

	// Detector Rail
	blockBehaviors[BLOCK_RAIL_DETECTOR].onBlockPlaced = [](WorldManager& _world, Int3 _pos, Entity& _placer,
	                                                       Cardinal::Value _face, BlockType _blockId,
	                                                       uint8_t _meta) -> bool {
		auto newPos = CanPlace(_world, _pos, _placer, _face, _blockId, _meta);
		if (newPos == Int3{ -1, -1, -1 })
			return false;
		if (!CanRailStay(_world, newPos, BLOCK_RAIL_DETECTOR))
			return false;
		return GenericPlace(_world, _pos, _placer, _face, _blockId, _meta);
	};

	blockBehaviors[BLOCK_RAIL_DETECTOR].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos,
	                                                               BlockType /*_blockId*/) -> void {
		if (!CanRailStay(_world, _pos, BLOCK_RAIL_DETECTOR)) {
			BreakAndDropBlock(_world, _pos);
			return;
		}
		RailManager::RefreshRail(_world, _pos, BLOCK_RAIL_DETECTOR, /*_forceWrite=*/false);
	};
	blockBehaviors[BLOCK_RAIL_DETECTOR].onBlockAdded = [](WorldManager& _world, Int3 _pos) -> void {
		UpdateRail(_world, _pos, BLOCK_RAIL_DETECTOR);
	};

	// Detector update behavior
	blockBehaviors[BLOCK_RAIL_DETECTOR].onEntityCollidedWithBlock = [](WorldManager& _world, Int3 _pos,
	                                                                   Entity& /*_entity*/) -> void {
		auto meta = _world.GetMetadata(_pos);
		if ((meta & 8) == 0)
			SetDetectorRailState(_world, _pos, meta);
	};

	blockBehaviors[BLOCK_RAIL_DETECTOR].onTick = [](WorldManager& _world, Int3 _pos, uint8_t _meta,
	                                                Java::Random& /*_random*/) -> void {
		if ((_meta & 8) != 0)
			SetDetectorRailState(_world, _pos, _meta);	
	};
		
	// Powered rail
	blockBehaviors[BLOCK_RAIL_POWERED].onBlockPlaced = [](WorldManager& _world, Int3 _pos, Entity& _placer,
	                                                      Cardinal::Value _face, BlockType _blockId,
	                                                      uint8_t _meta) -> bool {
		auto newPos = CanPlace(_world, _pos, _placer, _face, _blockId, _meta);
		if (newPos == Int3{ -1, -1, -1 })
			return false;
		if (!CanRailStay(_world, newPos, BLOCK_RAIL_POWERED))
			return false;
		bool canPlace = GenericPlace(_world, _pos, _placer, _face, _blockId, _meta);
		if (canPlace) {
			RailManager::UpdateRailPower(_world, _pos, BLOCK_RAIL_POWERED);
			return true;
		}
		return false;
	};
	blockBehaviors[BLOCK_RAIL_POWERED].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos,
	                                                              BlockType /*_blockId*/) -> void {
		if (!CanRailStay(_world, _pos, BLOCK_RAIL_POWERED)) {
			BreakAndDropBlock(_world, _pos);
			return;
		}
		RailManager::UpdateRailPower(_world, _pos, BLOCK_RAIL_POWERED);
	};
	blockBehaviors[BLOCK_RAIL_POWERED].onBlockAdded = [](WorldManager& _world, Int3 _pos) -> void {
		UpdateRail(_world, _pos, BLOCK_RAIL_POWERED);
	};
}

}; // namespace Blocks