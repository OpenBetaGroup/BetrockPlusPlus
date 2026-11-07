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
#include "enums/items.h"
#include "helpers/direction_fixer.h"
#include "helpers/java/java_math.h"
#include "internal.h"
#include "items/item_properties.h"
#include "logger.h"
#include "numeric_structs.h"
#include "packet_data.h"
#include "managers/redstone_manager.h"
#include "managers/tick_scheduler.h"
#include "tile_entities/tile_entity.h"
#include "world.h"

namespace Blocks {

static constexpr int MAX_PUSH_COUNT = 12;

static int GetDirectionFromYaw(float _yaw, int _directionCount) {
	return MathHelper::FloorDouble((_yaw * _directionCount / 360.0f) + 0.5f) & 3;
}

static Int3 GetDirectionVectorFromMeta(uint8_t _meta) {
	auto orientation = _meta & 7;
	Int3 lut[6] = { { 0, -1, 0 }, { 0, 1, 0 }, { 0, 0, -1 }, { 0, 0, 1 }, { -1, 0, 0 }, { 1, 0, 0 } };

	return lut[orientation % 6];
}

static bool PistonPowered(uint8_t _meta) {
	return (_meta & 8) != 0;
}

static bool CanPushBlock(WorldManager& _world, BlockType _block, Int3 _pos, bool _allowBreakable = true) {
	if (_block == BLOCK_OBSIDIAN)
		return false;

	// If we are a piston check if we are extended
	if (_block == BLOCK_PISTON || _block == BLOCK_PISTON_STICKY) {
		return !PistonPowered(_world.GetMetadata(_pos));
	}

	auto props = Blocks::blockProperties[_block];
	// TODO: Turn this into an enum
	PushabilityFlag mobilityFlag = props.pushMobilityOverride > 0 ? props.pushMobilityOverride
	                                                               : props.material.mobilityFlag;
	if (props.hardness == -1.0f || mobilityFlag == PushabilityFlag::Immovable ||
	    (!_allowBreakable && mobilityFlag == PushabilityFlag::NoPush))
		return false;

	// If there is a tile entity here then refuse
	auto te = _world.GetTileEntity(_pos);
	return te == nullptr;
}

static bool CanExtend(WorldManager& _world, Int3 _pos) {
	auto meta = _world.GetMetadata(_pos);

	auto dirVec = GetDirectionVectorFromMeta(meta);
	int pushCount = 0;

	Int3 checkPos = _pos + dirVec;

	while (pushCount <= MAX_PUSH_COUNT) {
		if (checkPos.y <= 0 || checkPos.y >= CHUNK_HEIGHT - 1)
			return false;

		auto blockId = _world.GetBlockId(checkPos);
		if (blockId != BLOCK_AIR) {
			if (!CanPushBlock(_world, blockId, checkPos))
				return false;

			auto props = Blocks::blockProperties[blockId];
			PushabilityFlag mobilityFlag = props.pushMobilityOverride > 0 ? props.pushMobilityOverride
			                                                      : props.material.mobilityFlag;
			if (mobilityFlag != PushabilityFlag::NoPush) {
				if (pushCount >= MAX_PUSH_COUNT)
					return false; // Too many blocks

				checkPos += dirVec;
				pushCount++;
				continue;
			}
		}

		return true;
	}

	return true;
}

// Java has one ignoreUpdates flag per Block instance, so one for regular and one for sticky
static bool ignoreUpdates[2] = { false, false };

static void SetMovingBlock(WorldManager& _world, Int3 _pos, uint8_t _meta, BlockType _stored, uint8_t _storedMeta,
                           int _orientation, bool _extending) {
	_world.SetBlock(_pos, BLOCK_PISTON_MOVING, _meta, false, false);

	auto pistonMov = std::make_shared<TileEntityPistonMoving>(_pos);
	pistonMov->extending = _extending;
	pistonMov->orientation = _orientation;
	pistonMov->storedBlock = _stored;
	pistonMov->storedMeta = _storedMeta;
	_world.CreateTileEntity(pistonMov);
}

static bool TryExtend(WorldManager& _world, Int3 _pos) {
	auto meta = _world.GetMetadata(_pos);
	auto myId = _world.GetBlockId(_pos);
	auto orientation = meta & 7;

	auto dirVec = GetDirectionVectorFromMeta(meta);
	int pushCount = 0;

	Int3 endPos = _pos + dirVec;

	while (pushCount <= MAX_PUSH_COUNT) {
		if (endPos.y <= 0 || endPos.y >= CHUNK_HEIGHT - 1)
			return false;

		auto blockId = _world.GetBlockId(endPos);
		if (blockId != BLOCK_AIR) {
			if (!CanPushBlock(_world, blockId, endPos))
				return false;
			auto props = Blocks::blockProperties[blockId];
			PushabilityFlag mobilityFlag = props.pushMobilityOverride > 0 ? props.pushMobilityOverride
			                                                      : props.material.mobilityFlag;
			if (mobilityFlag != PushabilityFlag::NoPush) {
				if (pushCount >= MAX_PUSH_COUNT)
					return false; // Too many blocks

				endPos += dirVec;
				pushCount++;
				continue;
			}

			// Breakable blocks get destroyed
			BreakAndDropBlock(_world, endPos);
		}
		break;
	}

	// Walk backwards from the end, turning each spot into a moving block
	while (endPos.x != _pos.x || endPos.y != _pos.y || endPos.z != _pos.z) {
		Int3 prevPos = endPos - dirVec;
		auto prevBlock = _world.GetBlockId(prevPos);
		auto prevMeta = _world.GetMetadata(prevPos);

		if (prevPos == _pos) {
			// The block behind us is our base, so this spot gets the head
			uint8_t headMeta = uint8_t(orientation | (myId == BLOCK_PISTON_STICKY ? 8 : 0));
			SetMovingBlock(_world, endPos, headMeta, BLOCK_PISTON_HEAD, headMeta, orientation, true);
		} else {
			SetMovingBlock(_world, endPos, prevMeta, prevBlock, prevMeta, orientation, true);
		}

		endPos = prevPos;
	}
	return true;
}

static void Retract(WorldManager& _world, Int3 _pos) {
	auto myId = _world.GetBlockId(_pos);
	int orientation = _world.GetMetadata(_pos) & 7;
	auto dirVec = GetDirectionVectorFromMeta(uint8_t(orientation));
	bool sticky = myId == BLOCK_PISTON_STICKY;
	bool& ignore = ignoreUpdates[sticky ? 1 : 0];
	Int3 headPos = _pos + dirVec;

	// If the head is still sliding out, snap it into place first
	if (auto te = _world.GetTileEntityShared<TileEntityPistonMoving>(headPos))
		te->InstantFinish(_world);

	// The base itself becomes a moving block while it retracts
	SetMovingBlock(_world, _pos, uint8_t(orientation), myId, uint8_t(orientation), orientation, false);

	if (sticky) {
		Int3 pullPos = headPos + dirVec;
		BlockType pullId = _world.GetBlockId(pullPos);
		uint8_t pullMeta = _world.GetMetadata(pullPos);
		bool wasMoving = false;

		if (pullId == BLOCK_PISTON_MOVING) {
			auto te = _world.GetTileEntityShared<TileEntityPistonMoving>(pullPos);
			if (te && te->orientation == orientation && te->extending) {
				te->InstantFinish(_world);
				pullId = te->storedBlock;
				pullMeta = te->storedMeta;
				wasMoving = true;
			}
		}
		auto props = Blocks::blockProperties[pullId];
		PushabilityFlag mobilityFlag = props.pushMobilityOverride > 0 ? props.pushMobilityOverride
		                                                      : props.material.mobilityFlag;
		bool cantPull = wasMoving || pullId == BLOCK_AIR || !CanPushBlock(_world, pullId, pullPos, false) ||
		                (mobilityFlag != PushabilityFlag::Normal && pullId != BLOCK_PISTON &&
		                 pullId != BLOCK_PISTON_STICKY);
		if (cantPull) {
			if (!wasMoving) {
				ignore = false;
				_world.SetBlock(headPos, BLOCK_AIR);
				ignore = true;
			}
		} else {
			ignore = false;
			_world.SetBlock(pullPos, BLOCK_AIR);
			ignore = true;
			SetMovingBlock(_world, headPos, pullMeta, pullId, pullMeta, orientation, false);
		}
	} else {
		ignore = false;
		_world.SetBlock(headPos, BLOCK_AIR);
		ignore = true;
	}
}

// Had to port this SPECIFICALLY for pistons, thanks notch
static bool IsProvidingPowerTo(WorldManager& _world, Int3 _src, Cardinal::Value _dir) {
	const BlockType id = _world.GetBlockId(_src);
	if (id == BLOCK_AIR)
		return false;

	// Normal cubes pass on whatever power they are receiving
	if (_world.IsBlockNormalCube(_src))
		return RedstoneManager::GetBlockPowerProfile(_world, _src).powered;

	const uint8_t meta = _world.GetMetadata(_src);
	switch (id) {
	case BLOCK_REDSTONE_TORCH_ON: {
		// Powers every side except the block it's attached to
		Cardinal::Value adir = Cardinal::Value::None;
		switch (meta) {
		case 1:
			adir = Cardinal::Value::West;
			break;
		case 2:
			adir = Cardinal::Value::East;
			break;
		case 3:
			adir = Cardinal::Value::North;
			break;
		case 4:
			adir = Cardinal::Value::South;
			break;
		case 5:
			adir = Cardinal::Value::Down;
			break;
		default:
			break;
		}
		return !(_dir == adir);
	}
	case BLOCK_LEVER:
	case BLOCK_BUTTON_STONE:
		return (meta & 8) != 0;
	case BLOCK_PRESSURE_PLATE_STONE:
	case BLOCK_PRESSURE_PLATE_WOOD:
		return meta > 0;
	case BLOCK_REDSTONE_REPEATER_ON:
		// Only powers the block it's facing
		switch (meta & 3) {
		case 0:
			return _dir == Cardinal::Value::North;
		case 1:
			return _dir == Cardinal::Value::East;
		case 2:
			return _dir == Cardinal::Value::South;
		default:
			return _dir == Cardinal::Value::West;
		}
	case BLOCK_REDSTONE: {
		if (meta == 0)
			return false;
		if (_dir == Cardinal::Value::Down)
			return true; // Dust always powers the block it sits on
		if (Cardinal::IsVertical(_dir))
			return false;
		auto c = RedstoneManager::GetRedstoneDustConnectivity(_world, _src);
		if (!c.powerX && !c.powerNX && !c.powerZ && !c.powerNZ)
			return true; // A dot powers all sides
		// A line powers the block it points into, unless it also turns sideways
		if (_dir == Cardinal::Value::East)
			return c.powerNX && !c.powerZ && !c.powerNZ;
		if (_dir == Cardinal::Value::West)
			return c.powerX && !c.powerZ && !c.powerNZ;
		if (_dir == Cardinal::Value::South)
			return c.powerNZ && !c.powerX && !c.powerNX;
		if (_dir == Cardinal::Value::North)
			return c.powerZ && !c.powerX && !c.powerNX;
		return false;
	}
	default:
		return false;
	}
}

static bool ShouldPistonBePowered(WorldManager& _world, Int3 _pos, int _orientation) {
	// Every side except the one the piston faces
	for (int side = 0; side < 6; side++) {
		if (side == _orientation)
			continue;
		const auto dir = GetDirectionFromMeta(BLOCK_PISTON, side);
		Int3 src = _pos.WithOffset(dir);
		if (IsProvidingPowerTo(_world, src, Cardinal::Opposite(dir)))
			return true;
	}

	// Anything that would power the block above the piston
	const Int3 above = _pos.WithOffset(Cardinal::Value::Up);
	for (int side = 1; side < 6; side++) {
		const auto dir = GetDirectionFromMeta(BLOCK_PISTON, side);
		Int3 src = above.WithOffset(dir);
		if (IsProvidingPowerTo(_world, src, Cardinal::Opposite(dir)))
			return true;
	}

	return false;
}

static void UpdatePistonState(WorldManager& _world, Int3 _pos) {
	auto myId = _world.GetBlockId(_pos);
	if (myId != BLOCK_PISTON && myId != BLOCK_PISTON_STICKY)
		return;
	auto meta = _world.GetMetadata(_pos);
	auto orientation = meta & 7;
	if (orientation > 5)
		return;
	auto shouldBeExtended = ShouldPistonBePowered(_world, _pos, orientation);
	bool& ignore = ignoreUpdates[myId == BLOCK_PISTON_STICKY ? 1 : 0];

	// Extend
	if (shouldBeExtended && !PistonPowered(meta)) {
		if (CanExtend(_world, _pos)) {
			ignore = true;
			if (TryExtend(_world, _pos))
				_world.SetMeta(_pos, orientation | 8);
			ignore = false;
			_world.PlayNoteAt(_pos, 0, orientation);
		}
		return;
	}

	// Retract
	if (!shouldBeExtended && PistonPowered(meta)) {
		_world.SetBlockRaw(_pos, myId, uint8_t(orientation));
		ignore = true;
		Retract(_world, _pos);
		ignore = false;
		_world.PlayNoteAt(_pos, 1, orientation);
		return;
	}
}

// Pistons are stupidly complicated..
// So they get their own file!
void RegisterPistonBehaviors() {
	blockBehaviors[BlockType::BLOCK_PISTON_HEAD] = {
		.getSelectionBox = PistonHeadAabb,
		.getRayBounds = PistonHeadAabb,
		.getCollider = PistonHeadCollider,
	};

	// Pistons
	auto onPistonPlace = [](WorldManager& _world, Int3 _pos, Entity& _placer, Cardinal::Value _face,
	                        BlockType _blockId, uint8_t /*_meta*/) -> bool {
		uint8_t orientation;

		if (std::abs(_placer.position.x - _pos.x) < 2.0 && std::abs(_placer.position.z - _pos.z) < 2.0) {
			double eyeY = _placer.position.y + 1.82 - _placer.yOffset;
			if (eyeY - _pos.y > 2.0) {
				orientation = 1; // up
			} else if (_pos.y - eyeY > 0.0) {
				orientation = 0; // down
			} else {
				int meta[] = { 2, 5, 3, 4 };
				orientation = meta[GetDirectionFromYaw(_placer.rotationYaw, 4)];
			}
		} else {
			int meta[] = { 2, 5, 3, 4 };
			orientation = meta[GetDirectionFromYaw(_placer.rotationYaw, 4)];
		}

		return GenericPlace(_world, _pos, _placer, _face, _blockId, orientation);
	};

	blockBehaviors[BLOCK_PISTON].onBlockPlaced = onPistonPlace;
	blockBehaviors[BLOCK_PISTON_STICKY].onBlockPlaced = onPistonPlace;

	blockBehaviors[BLOCK_PISTON].onBlockAdded = [](WorldManager& _world, Int3 _pos) -> void {
		if (_world.GetTileEntity(_pos) == nullptr)
			UpdatePistonState(_world, _pos);
	};
	blockBehaviors[BLOCK_PISTON_STICKY].onBlockAdded = [](WorldManager& _world, Int3 _pos) -> void {
		if (_world.GetTileEntity(_pos) == nullptr)
			UpdatePistonState(_world, _pos);
	};

	// Neighbor updates
	blockBehaviors[BLOCK_PISTON].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos,
	                                                        BlockType /*_blockId*/) -> void {
		if (!ignoreUpdates[0])
			UpdatePistonState(_world, _pos);
	};
	blockBehaviors[BLOCK_PISTON_STICKY].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos,
	                                                               BlockType /*_blockId*/) -> void {
		if (!ignoreUpdates[1])
			UpdatePistonState(_world, _pos);
	};

	// This removes any invalid heads
	blockBehaviors[BLOCK_PISTON_HEAD].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos,
	                                                             BlockType /*_blockId*/) -> void {
		int orientation = _world.GetMetadata(_pos) & 7;
		if (orientation > 5)
			return;
		Int3 basePos = _pos - GetDirectionVectorFromMeta(uint8_t(orientation));
		BlockType baseId = _world.GetBlockId(basePos);

		if (baseId != BLOCK_PISTON && baseId != BLOCK_PISTON_STICKY) {
			_world.SetBlock(_pos, BLOCK_AIR);
			return;
		}

		// Forward to the base, respecting its ignore flag like Java does
		if (!ignoreUpdates[baseId == BLOCK_PISTON_STICKY ? 1 : 0])
			UpdatePistonState(_world, basePos);
	};

	// Breaking the head breaks the base
	blockBehaviors[BLOCK_PISTON_HEAD].onBlockDestroyedByPlayer = [](WorldManager& _world, Int3 _pos,
	                                                                Entity& _destroyer) -> void {
		uint8_t meta = _world.GetMetadata(_pos);
		GenericBreak(_world, _pos, _destroyer);

		Int3 basePos = _pos - GetDirectionVectorFromMeta(meta);
		BlockType baseId = _world.GetBlockId(basePos);
		if ((baseId == BLOCK_PISTON || baseId == BLOCK_PISTON_STICKY) && PistonPowered(_world.GetMetadata(basePos)))
			BreakAndDropBlock(_world, basePos);
	};

	// So moving blocks still drop
	blockBehaviors[BLOCK_PISTON_MOVING].onBlockDestroyedByExplosion = [](WorldManager& _world, Int3 _pos) -> void {
		BlockType stored = BLOCK_AIR;
		uint8_t storedMeta = 0;
		if (auto te = _world.GetTileEntityAs<TileEntityPistonMoving>(_pos)) {
			stored = te->storedBlock;
			storedMeta = te->storedMeta;
		}

		_world.SetBlock(_pos, BLOCK_AIR);

		if (stored != BLOCK_AIR && stored != BLOCK_PISTON_MOVING)
			DropBlockItemsWithChance(_world, _pos, stored, storedMeta, 1.0f);
	};
}
}; // namespace Blocks