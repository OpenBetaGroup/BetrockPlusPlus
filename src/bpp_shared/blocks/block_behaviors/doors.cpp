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
#include "numeric_structs.h"
#include "packet_data.h"
#include "managers/rail_manager.h"
#include "managers/redstone_manager.h"
#include "managers/tick_scheduler.h"
#include "tile_entities/tile_entity.h"
#include "world.h"

namespace Blocks {

static void ToggleTrapdoor(WorldManager& _world, Int3 _pos, PlayerSession* _triggeringSession) {
	auto meta = _world.GetMetadata(_pos);
	_world.SetMeta(_pos, uint8_t(meta ^ 0b100)); // XOR bit 2; flips open/closed
	if (_world.onWorldEvent)
		_world.onWorldEvent(PacketData::WorldEvent::DOOR_TOGGLE, _pos, 0, _triggeringSession);
	return;
}

static void ToggleDoor(WorldManager& _world, Int3 _pos, PlayerSession* _triggeringSession, BlockType _doorType) {
	auto meta = _world.GetMetadata(_pos);
	if (meta & 8) {
		if (_world.GetBlockId({ _pos.x, _pos.y - 1, _pos.z }) != _doorType)
			return;
		blockBehaviors[_doorType].onBlockActivated(_world, { _pos.x, _pos.y - 1, _pos.z }, _triggeringSession);
		return;
	}

	Int3 top = { _pos.x, _pos.y + 1, _pos.z };
	uint8_t newBottomMeta = uint8_t(meta ^ 0b100);

	_world.SetMeta(_pos, newBottomMeta);

	if (_world.GetBlockId(top) == _doorType && (_world.GetMetadata(top) & 8))
		_world.SetMeta(top, uint8_t(newBottomMeta + 8));

	if (_world.onWorldEvent)
		_world.onWorldEvent(PacketData::WorldEvent::DOOR_TOGGLE, _pos, 0, _triggeringSession);
}

static void BreakDoor(WorldManager& _world, Int3 _pos, BlockType _doorType) {
	auto meta = _world.GetMetadata(_pos);
	if (meta & 8) {
		// Offset post down
		_pos.Offset(Cardinal::Value::Down);
		// We are the top of the door
		if (_world.GetBlockId(_pos) != _doorType) {
			// Below us is not the bottom of a door! This is bad!
			_world.SetBlock(_pos.WithOffset(Cardinal::Value::Up), BLOCK_AIR);
			return;
		}
	}
	// Since we're now guaranteed to be
	// pointing at the bottom of the door,
	// we can continue like this
	Int3 top = _pos.WithOffset(Cardinal::Value::Up);
	if (_world.GetBlockId(top) == _doorType && (_world.GetMetadata(top) & 8)) {
		_world.SetBlock(top, BLOCK_AIR);
	}
	BreakAndDropBlock(_world, _pos);
}

static void NeighborUpdateDoor(WorldManager& _world, Int3 _pos, BlockType _blockId) {
	const auto myType = _world.GetBlockId(_pos);
	if (myType != BLOCK_DOOR_IRON && myType != BLOCK_DOOR_WOOD)
		return;
	const uint8_t meta = _world.GetMetadata(_pos);
	const bool fromPower = RedstoneManager::CanProvidePower(_blockId);

	if (meta & 8) {
		// If the bottom half is gone then break
		const Int3 below = _pos.WithOffset(Cardinal::Value::Down);
		if (_world.GetBlockId(below) != myType) {
			BreakDoor(_world, _pos, myType);
			return;
		}
		if (fromPower)
			NeighborUpdateDoor(_world, below, _blockId);
		return;
	}

	// If the top half is gone or we don't have support then break
	const Int3 above = _pos.WithOffset(Cardinal::Value::Up);
	if (_world.GetBlockId(above) != myType || !_world.IsBlockNormalCube(_pos.WithOffset(Cardinal::Value::Down))) {
		BreakDoor(_world, _pos, myType);
		return;
	}

	if (!fromPower)
		return;
	const bool isOpen = (meta >> 2) & 1;
	const bool powered = RedstoneManager::IsPositionPowered(_world, _pos) ||
	                     RedstoneManager::IsPositionPowered(_world, above);
	if (powered != isOpen)
		ToggleDoor(_world, _pos, nullptr, myType);
}

void RegisterDoorBehaviors() {
	blockBehaviors[BlockType::BLOCK_DOOR_WOOD] = {
		.getSelectionBox = DoorAabb,
		.getRayBounds = DoorAabb,
		.getCollider = DoorCollider,
	};
	blockBehaviors[BlockType::BLOCK_DOOR_IRON] = {
		.getSelectionBox = DoorAabb,
		.getRayBounds = DoorAabb,
		.getCollider = DoorCollider,
	};

	blockBehaviors[BlockType::BLOCK_TRAPDOOR] = {
		.getSelectionBox = TrapdoorAabb,
		.getRayBounds = TrapdoorAabb,
		.getCollider = TrapdoorCollider,
		.onBlockClicked = ToggleTrapdoor,
		.onBlockActivated = [](WorldManager& _world, Int3 _pos, PlayerSession* _triggeringSession) -> bool {
		    ToggleTrapdoor(_world, _pos, _triggeringSession);
		    return false;
		},
		.onBlockPlaced = [](WorldManager& _world, Int3 _pos, Entity& _placer, Cardinal::Value _face,
		                    BlockType /*_blockId*/, uint8_t /*_meta*/) -> bool {
		    // Doors can only be placed against the sides of blocks
		    if (_face == Cardinal::Value::Up || _face == Cardinal::Value::Down)
			    return false;

			// Pop off if we aren't supported
		    auto meta = GetMetaFromDirection(BLOCK_TRAPDOOR, Cardinal::Opposite(_face));
		    auto direction = GetDirectionFromMeta(BLOCK_TRAPDOOR, meta);
		    auto supportPos = _pos.WithOffset(direction);

		    if (!_world.IsBlockNormalCube(supportPos))
			    return false;

		    if (!IsReplaceable(_world, _pos))
			    return false;

		    return GenericPlace(_world, _pos, _placer, _face, BLOCK_TRAPDOOR, meta);
		},
	};

	auto onDoorPlace = [](WorldManager& _world, Int3 _pos, Entity& _placer, Cardinal::Value _face, BlockType _blockId,
	                      uint8_t /*_meta*/) -> bool {
		// Doors can only be placed by clicking the top face of a block
		if (_face != Cardinal::Value::Up)
			return false;

		// _pos is already the target cell
		const Int3 placePos = _pos;
		const Int3 abovePos = placePos.WithOffset(Cardinal::Value::Up);

		// Is this placement valid?
		if (!_world.InBounds(abovePos.y))
			return false;
		if (!_world.IsBlockNormalCube(placePos.WithOffset(Cardinal::Value::Down)))
			return false;
		if (!IsReplaceable(_world, placePos) || !IsReplaceable(_world, abovePos))
			return false;

		auto facing = Cardinal::FromAngle(_placer.rotationYaw);

		const Int3 left = placePos.WithOffset(Cardinal::ToLeft(facing));
		const Int3 leftTop = left.WithOffset(Cardinal::Value::Up);

		const Int3 right = placePos.WithOffset(Cardinal::ToRight(facing));
		const Int3 rightTop = right.WithOffset(Cardinal::Value::Up);

		const int leftSolidBlocks = (_world.IsBlockNormalCube(left) ? 1 : 0) +
		                            (_world.IsBlockNormalCube(leftTop) ? 1 : 0);
		const int rightSolidBlocks = (_world.IsBlockNormalCube(right) ? 1 : 0) +
		                             (_world.IsBlockNormalCube(rightTop) ? 1 : 0);

		const bool leftHasDoor = _world.GetBlockId(left) == _blockId || _world.GetBlockId(leftTop) == _blockId;
		const bool rightHasDoor = _world.GetBlockId(right) == _blockId || _world.GetBlockId(rightTop) == _blockId;

		bool hingeOnRight = false;
		if (leftHasDoor && !rightHasDoor) {
			hingeOnRight = true;
		} else if (rightSolidBlocks > leftSolidBlocks) {
			hingeOnRight = true;
		}

		if (hingeOnRight)
			facing = Cardinal::ToLeft(facing);

		uint8_t meta = GetMetaFromDirection(_blockId, facing);
		if (hingeOnRight)
			meta |= 4;

		_world.SetBlock(placePos, _blockId, meta);
		_world.SetBlock(abovePos, _blockId, uint8_t(meta | 8));

		return true;
	};

	blockBehaviors[BLOCK_DOOR_WOOD].onBlockPlaced = onDoorPlace;
	blockBehaviors[BLOCK_DOOR_IRON].onBlockPlaced = onDoorPlace;

	blockBehaviors[BLOCK_DOOR_WOOD].onBlockActivated = [](WorldManager& _world, Int3 _pos,
	                                                      PlayerSession* _triggeringSession) -> bool {
		ToggleDoor(_world, _pos, _triggeringSession, BLOCK_DOOR_WOOD);
		return false;
	};
	blockBehaviors[BLOCK_DOOR_WOOD].onBlockClicked = [](WorldManager& _world, Int3 _pos,
	                                                    PlayerSession* _triggeringSession) -> void {
		ToggleDoor(_world, _pos, _triggeringSession, BLOCK_DOOR_WOOD);
	};
	blockBehaviors[BLOCK_DOOR_WOOD].onBlockDestroyedByPlayer = [](WorldManager& _world, Int3 _pos,
	                                                              Entity& /*_destroyer*/) {
		BreakDoor(_world, _pos, BLOCK_DOOR_WOOD);
	};
	blockBehaviors[BLOCK_DOOR_IRON].onBlockDestroyedByPlayer = [](WorldManager& _world, Int3 _pos,
	                                                              Entity& /*_destroyer*/) {
		BreakDoor(_world, _pos, BLOCK_DOOR_IRON);
	};
	blockBehaviors[BLOCK_DOOR_IRON].onBlockDestroyedByExplosion = [](WorldManager& _world, Int3 _pos) {
		BreakDoor(_world, _pos, BLOCK_DOOR_IRON);
	};
	blockBehaviors[BLOCK_DOOR_WOOD].onBlockDestroyedByExplosion = [](WorldManager& _world, Int3 _pos) {
		BreakDoor(_world, _pos, BLOCK_DOOR_WOOD);
	};

	blockBehaviors[BLOCK_DOOR_WOOD].onNeighborBlockChange = NeighborUpdateDoor;
	blockBehaviors[BLOCK_DOOR_IRON].onNeighborBlockChange = NeighborUpdateDoor;

	blockBehaviors[BLOCK_TRAPDOOR].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos,
	                                                          BlockType _blockId) -> void {
		// Pop off if we aren't supported
		auto meta = _world.GetMetadata(_pos);
		auto direction = GetDirectionFromMeta(BLOCK_TRAPDOOR, meta);
		auto supportPos = _pos.WithOffset(direction);

		if (!_world.IsBlockNormalCube(supportPos)) {
			BreakAndDropBlock(_world, _pos);
			return;
		}

		// Check to see if the updater is from a redstone component
		if (!RedstoneManager::CanProvidePower(_blockId))
			return;

		// Clear top-most bit
		const bool isOpen = (meta >> 2) & 1;
		const bool powered = RedstoneManager::IsPositionPowered(_world, _pos);
		if (powered != isOpen)
			ToggleTrapdoor(_world, _pos, nullptr);
	};

	blockBehaviors[BLOCK_TRAPDOOR].onBlockClicked = ToggleTrapdoor;
}

}; // namespace Blocks
