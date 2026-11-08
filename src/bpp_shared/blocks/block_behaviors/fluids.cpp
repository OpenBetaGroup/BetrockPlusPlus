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

static bool IsBurningMaterial(WorldManager& _world, Int3 _pos) {
	switch (_world.GetMaterial(_pos).type) {
	case MaterialType::Wood:
	case MaterialType::Leaves:
	case MaterialType::Cloth:
	case MaterialType::TNT:
		return true;
	default:
		return false;
	}
}

static void TryLavaHarden(WorldManager& _world, Int3 _pos) {
	// Make sure we are lava
	if (_world.GetMaterial(_pos).type != MaterialType::Lava)
		return;

	bool canHarden = false;

	// Check around and above us
	int d[4] = { -1, 1, 0, 0 };
	for (int i = 0; i < 4; i++) {
		int dx = _pos.x + d[i];
		int dz = _pos.z + d[3 - i];
		canHarden = _world.GetMaterial({ dx, _pos.y, dz }).type == MaterialType::Water;
		if (canHarden)
			break;
	}

	// Check above
	if (!canHarden)
		canHarden = _world.GetMaterial(_pos.WithOffset(Cardinal::Value::Up)).type == MaterialType::Water;

	// We can harden
	if (canHarden) {
		auto myLevel = _world.GetMetadata(_pos);
		if (myLevel == 0) {
			// Obsidian
			_world.SetBlock(_pos, BLOCK_OBSIDIAN);
		} else if (myLevel > 0 && myLevel <= 4) {
			_world.SetBlock(_pos, BLOCK_COBBLESTONE);
		}
	}
}

static bool BlocksFlow(BlockType _block) {
	auto blockMaterial = blockProperties[_block].material;
	if (_block == BLOCK_DOOR_WOOD || _block == BLOCK_DOOR_IRON || _block == BLOCK_SIGN_STANDING ||
	    _block == BLOCK_LADDER || _block == BLOCK_SUGARCANE)
		return true;
	return blockMaterial.isSolid;
}

static bool IsDisplaceable(BlockType _block, MaterialType _fluidMaterialType) {
	auto blockMaterial = blockProperties[_block].material;
	if (blockMaterial.type == _fluidMaterialType)
		return false;
	if (blockMaterial.type == MaterialType::Lava)
		return false;
	return !BlocksFlow(_block);
}

static bool IsOpenForFlow(WorldManager& _world, Int3 _pos, MaterialType _fluidMaterialType) {
	auto block = _world.GetBlockId(_pos);
	auto material = _world.GetMaterial(_pos);
	return !BlocksFlow(block) && (material.type != _fluidMaterialType || _world.GetMetadata(_pos) != 0);
}

static constexpr Cardinal::Value FLOW_DIR[4] = { Cardinal::Value::West, Cardinal::Value::East, Cardinal::Value::North,
	                                             Cardinal::Value::South };

static int CalculateFlowCost(WorldManager& _world, Int3 _pos, int _depth, int _cameFrom,
                             MaterialType _fluidMaterialType) {
	int lowest = 1000;
	for (int i = 0; i < 4; i++) {
		if ((i == 0 && _cameFrom == 1) || (i == 1 && _cameFrom == 0) || (i == 2 && _cameFrom == 3) ||
		    (i == 3 && _cameFrom == 2))
			continue;

		Int3 neighborPos = _pos.WithOffset(FLOW_DIR[i]);
		if (!IsOpenForFlow(_world, neighborPos, _fluidMaterialType))
			continue;

		if (!BlocksFlow(_world.GetBlockId(neighborPos.WithOffset(Cardinal::Value::Down))))
			return _depth;

		if (_depth >= 4)
			continue;

		int cost = CalculateFlowCost(_world, neighborPos, _depth + 1, i, _fluidMaterialType);
		if (cost < lowest)
			lowest = cost;
	}
	return lowest;
}

// Vanilla getFlowDecay: -1 if the block isn't this fluid, otherwise its metadata
static int GetFlowDecay(WorldManager& _world, Int3 _pos, MaterialType _fluidMaterialType) {
	if (_world.GetMaterial(_pos).type != _fluidMaterialType)
		return -1;
	return _world.GetMetadata(_pos);
}

static void FlowingFluidTick(WorldManager& _world, Int3 _pos, BlockType _flowingId, BlockType _stillId,
                             MaterialType _fluid, int _tickRate, Java::Random& _random) {
	const bool isLava = _fluid == MaterialType::Lava;
	const int decayStep = (isLava && _world.GetDimension() != Dimension::Nether) ? 2 : 1;
	const Int3 belowPos = _pos.WithOffset(Cardinal::Value::Down);

	int level = GetFlowDecay(_world, _pos, _fluid);
	if (level < 0)
		return; // not this fluid anymore
	bool settle = true;

	if (level > 0) {
		// getSmallestFlowDecay over the four sides, counting adjacent sources
		int smallest = -100;
		int adjacentSources = 0;
		for (int i = 0; i < 4; i++) {
			int decay = GetFlowDecay(_world, _pos.WithOffset(FLOW_DIR[i]), _fluid);
			if (decay < 0)
				continue;
			if (decay == 0)
				adjacentSources++;
			if (decay >= 8)
				decay = 0;
			if (!(smallest >= 0 && decay >= smallest))
				smallest = decay;
		}

		int newLevel = smallest + decayStep;
		if (newLevel >= 8 || smallest < 0)
			newLevel = -1;

		// Fed from above
		int above = GetFlowDecay(_world, _pos.WithOffset(Cardinal::Value::Up), _fluid);
		if (above >= 0)
			newLevel = above >= 8 ? above : above + 8;

		// Infinite water
		if (adjacentSources >= 2 && !isLava) {
			Material belowMaterial = _world.GetMaterial(belowPos);
			if (belowMaterial.isSolid)
				newLevel = 0;
			else if (belowMaterial.type == _fluid && _world.GetMetadata(_pos) == 0)
				newLevel = 0;
		}

		// Lava hesitation
		if (isLava && level < 8 && newLevel < 8 && newLevel > level && _random.NextInt(4) != 0) {
			newLevel = level;
			settle = false;
		}

		if (newLevel != level) {
			level = newLevel;
			if (level < 0) {
				_world.SetBlock(_pos, BLOCK_AIR);
				// Vanilla keeps going with level == -1 (see the downward flow below)
			} else {
				_world.SetMeta(_pos, uint8_t(level));
				_world.tickScheduler.ScheduleUpdateTick(_pos, _flowingId, _tickRate);
				_world.NotifyNeighborsOfUpdate(_pos, _flowingId);
			}
		} else if (settle) {
			_world.SetBlockRaw(_pos, _stillId, uint8_t(level));
			if (isLava)
				TryLavaHarden(_world, _pos);
		}
	} else {
		_world.SetBlockRaw(_pos, _stillId, uint8_t(level));
		if (isLava)
			TryLavaHarden(_world, _pos);
	}

	// Flow down
	if (IsDisplaceable(_world.GetBlockId(belowPos), _fluid)) {
		_world.SetBlock(belowPos, _flowingId, uint8_t(level >= 8 ? level : level + 8));
		return;
	}

	// Spread sideways if we're a source or something below us blocks the flow
	if (level < 0 || (level != 0 && !BlocksFlow(_world.GetBlockId(belowPos))))
		return;

	// getOptimalFlowDirections
	int costs[4];
	for (int i = 0; i < 4; i++) {
		costs[i] = 1000;
		Int3 neighborPos = _pos.WithOffset(FLOW_DIR[i]);
		if (!IsOpenForFlow(_world, neighborPos, _fluid))
			continue;
		if (!BlocksFlow(_world.GetBlockId(neighborPos.WithOffset(Cardinal::Value::Down))))
			costs[i] = 0;
		else
			costs[i] = CalculateFlowCost(_world, neighborPos, 1, i, _fluid);
	}
	int minCost = costs[0];
	for (int i = 1; i < 4; i++)
		if (costs[i] < minCost)
			minCost = costs[i];

	int outLevel = level >= 8 ? 1 : level + decayStep;
	if (outLevel >= 8)
		return;

	for (int i = 0; i < 4; i++) {
		if (costs[i] != minCost)
			continue;
		Int3 newPos = _pos.WithOffset(FLOW_DIR[i]);
		if (!IsDisplaceable(_world.GetBlockId(newPos), _fluid))
			continue;
		if (!isLava)
			BreakAndDropBlock(_world, newPos);
		_world.SetBlock(newPos, _flowingId, uint8_t(outLevel));
	}
}

static Vec3 GetFluidFlowVector(WorldManager& _world, Int3 _pos) {
	auto waterMaterial = Material::Water();
	Vec3 flowVector{};
	auto getEffectiveFlowDecay = [&](WorldManager& _lWorld, Int3 _lPos, Material _lMaterial) {
		if (_lWorld.GetMaterial(_lPos) != _lMaterial)
			return -1;
		int meta = _lWorld.GetMetadata(_lPos);
		if (meta >= 8)
			meta = 0;

		return meta;
	};

	int myFlowContribution = getEffectiveFlowDecay(_world, _pos, waterMaterial);

	// Same as FLOW_DIR, so could maybe be reused for that?
	static constexpr Cardinal::Value NEIGHBOR_BLOCKS[4] = { Cardinal::Value::West, Cardinal::Value::East,
		                                                    Cardinal::Value::North, Cardinal::Value::South };
	// Get the contribution of our horizontal neighbors
	for (int i = 0; i < 4; i++) {
		Int3 neighborPos = _pos.WithOffset(NEIGHBOR_BLOCKS[i]);
		int neighborFlowContribution = getEffectiveFlowDecay(_world, neighborPos, waterMaterial);
		int flowDifference = 0;
		// Our neighbor block didn't have the same material
		if (neighborFlowContribution < 0) {
			if (!_world.GetMaterial(neighborPos).isSolid) {
				// Check the block below us to see if its water, if it is, STRONGLY pull down
				int belowFlowContribution = getEffectiveFlowDecay(_world, neighborPos.WithOffset(Cardinal::Value::Down),
				                                                  waterMaterial);
				if (belowFlowContribution >= 0) {
					flowDifference = belowFlowContribution - (myFlowContribution - 8);
					flowVector.x += double((neighborPos.x - _pos.x) * flowDifference);
					flowVector.z += double((neighborPos.z - _pos.z) * flowDifference);
				}
			}
		} else {
			flowDifference = neighborFlowContribution - myFlowContribution;
			flowVector.x += double((neighborPos.x - _pos.x) * flowDifference);
			flowVector.z += double((neighborPos.z - _pos.z) * flowDifference);
		}
	}

	auto isFluidWall = [&](Int3 _checkPos) {
		Material neighborMaterial = _world.GetMaterial(_checkPos);
		if (neighborMaterial == waterMaterial)
			return false;
		if (neighborMaterial == Material::Ice())
			return false;
		return neighborMaterial.isSolid;
	};

	// If we're a falling fluid segment, check whether we're clinging to a wall
	if (_world.GetMetadata(_pos) >= 8) {
		bool nearWall = false;

		if (!nearWall && isFluidWall(_pos.WithOffset(Cardinal::Value::North)))
			nearWall = true;
		if (!nearWall && isFluidWall(_pos.WithOffset(Cardinal::Value::South)))
			nearWall = true;
		if (!nearWall && isFluidWall(_pos.WithOffset(Cardinal::Value::West)))
			nearWall = true;
		if (!nearWall && isFluidWall(_pos.WithOffset(Cardinal::Value::East)))
			nearWall = true;
		if (!nearWall && isFluidWall(_pos.WithOffset(Cardinal::Value::Up).Offset(Cardinal::Value::North)))
			nearWall = true;
		if (!nearWall && isFluidWall(_pos.WithOffset(Cardinal::Value::Up).Offset(Cardinal::Value::South)))
			nearWall = true;
		if (!nearWall && isFluidWall(_pos.WithOffset(Cardinal::Value::Up).Offset(Cardinal::Value::West)))
			nearWall = true;
		if (!nearWall && isFluidWall(_pos.WithOffset(Cardinal::Value::Up).Offset(Cardinal::Value::East)))
			nearWall = true;

		if (nearWall) {
			double lenSq = flowVector.x * flowVector.x + flowVector.y * flowVector.y + flowVector.z * flowVector.z;
			if (lenSq > 0.0) {
				double invLen = 1.0 / std::sqrt(lenSq);
				flowVector.x *= invLen;
				flowVector.y *= invLen;
				flowVector.z *= invLen;
			}
			flowVector.y += -6.0;
		}
	}

	double lenSq = flowVector.x * flowVector.x + flowVector.y * flowVector.y + flowVector.z * flowVector.z;
	if (lenSq > 0.0) {
		double invLen = 1.0 / std::sqrt(lenSq);
		flowVector.x *= invLen;
		flowVector.y *= invLen;
		flowVector.z *= invLen;
	}

	return flowVector;
}

void RegisterFluidBehaviors() {
	// Liquids/zero-size AABBs
	blockBehaviors[BlockType::BLOCK_WATER_FLOWING] = {
		.getSelectionBox = LiquidAabb,
		.getCollider = EmptyCollider,
	};
	blockBehaviors[BlockType::BLOCK_WATER_STILL] = {
		.getSelectionBox = LiquidAabb,
		.getCollider = EmptyCollider,
	};
	blockBehaviors[BlockType::BLOCK_LAVA_FLOWING] = {
		.getSelectionBox = LiquidAabb,
		.getCollider = EmptyCollider,
	};
	blockBehaviors[BlockType::BLOCK_LAVA_STILL] = {
		.getSelectionBox = LiquidAabb,
		.getCollider = EmptyCollider,
	};
	// Specific behavioral overrides
	blockBehaviors[BLOCK_WATER_FLOWING].velocityToAddToEntity = [](WorldManager& _world, Int3 _pos,
	                                                               Vec3& _pushVector) -> void {
		Vec3 flowVector = GetFluidFlowVector(_world, _pos);
		_pushVector = _pushVector + flowVector;
	};
	blockBehaviors[BLOCK_WATER_STILL].velocityToAddToEntity = [](WorldManager& _world, Int3 _pos,
	                                                             Vec3& _pushVector) -> void {
		Vec3 flowVector = GetFluidFlowVector(_world, _pos);
		_pushVector = _pushVector + flowVector;
	};
	// Lava physics
	blockBehaviors[BLOCK_LAVA_FLOWING].onBlockAdded = [](WorldManager& _world, Int3 _pos) -> void {
		// Schedule ourselves for an update
		_world.tickScheduler.ScheduleUpdateTick(_pos, BLOCK_LAVA_FLOWING,
		                                        _world.GetDimension() == Dimension::Nether ? 10 : 30);
		TryLavaHarden(_world, _pos);
	};
	blockBehaviors[BLOCK_LAVA_FLOWING].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos,
	                                                              BlockType /*_blockId*/) -> void {
		// Stack overflow if this was regular set block!
		_world.tickScheduler.ScheduleUpdateTick(_pos, BLOCK_LAVA_FLOWING,
		                                        _world.GetDimension() == Dimension::Nether ? 10 : 30);
		TryLavaHarden(_world, _pos);
	};
	blockBehaviors[BLOCK_LAVA_STILL].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos,
	                                                            BlockType /*_blockId*/) -> void {
		// Stack overflow if this was regular set block!
		_world.SetBlockRaw(_pos, BLOCK_LAVA_FLOWING, _world.GetMetadata(_pos));
		_world.tickScheduler.ScheduleUpdateTick(_pos, BLOCK_LAVA_FLOWING,
		                                        _world.GetDimension() == Dimension::Nether ? 10 : 30);
		TryLavaHarden(_world, _pos);
	};
	blockBehaviors[BLOCK_LAVA_STILL].onBlockAdded = [](WorldManager& _world, Int3 _pos) -> void {
		TryLavaHarden(_world, _pos);
	};
	blockBehaviors[BLOCK_LAVA_FLOWING].onTick = [](WorldManager& _world, Int3 _pos, uint8_t /*_meta*/,
	                                               Java::Random& _random) -> void {
		FlowingFluidTick(_world, _pos, BLOCK_LAVA_FLOWING, BLOCK_LAVA_STILL, MaterialType::Lava,
		                 _world.GetDimension() == Dimension::Nether ? 10 : 30, _random);
	};
	blockBehaviors[BLOCK_LAVA_STILL].onTick = [](WorldManager& _world, Int3 _pos, uint8_t /*_meta*/,
	                                             Java::Random& _random) -> void {
		if (!Gamerule::Get<bool>(Gamerule::Id::FireSpread))
			return;
		Int3 p = _pos;
		int tries = _random.NextInt(3);
		for (int i = 0; i < tries; i++) {
			p.x += _random.NextInt(3) - 1;
			p.y += 1;
			p.z += _random.NextInt(3) - 1;
			BlockType id = _world.GetBlockId(p);
			if (id == BLOCK_AIR) {
				if (IsBurningMaterial(_world, p.WithOffset(Cardinal::Value::West)) ||
				    IsBurningMaterial(_world, p.WithOffset(Cardinal::Value::East)) ||
				    IsBurningMaterial(_world, p.WithOffset(Cardinal::Value::North)) ||
				    IsBurningMaterial(_world, p.WithOffset(Cardinal::Value::South)) ||
				    IsBurningMaterial(_world, p.WithOffset(Cardinal::Value::Down)) ||
				    IsBurningMaterial(_world, p.WithOffset(Cardinal::Value::Up))) {
					_world.SetBlock(p, BLOCK_FIRE);
					return;
				}
			} else if (Blocks::blockProperties[id].material.isSolid) {
				return;
			}
		}
	};

	// FLUID PHYSICS (water)
	blockBehaviors[BLOCK_WATER_FLOWING].onBlockAdded = [](WorldManager& _world, Int3 _pos) -> void {
		// Schedule ourselves for an update
		_world.tickScheduler.ScheduleUpdateTick(_pos, BLOCK_WATER_FLOWING, 5);
	};
	blockBehaviors[BLOCK_WATER_FLOWING].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos,
	                                                               BlockType /*_blockId*/) -> void {
		// Stack overflow if this was regular set block!
		_world.tickScheduler.ScheduleUpdateTick(_pos, BLOCK_WATER_FLOWING, 5);
	};
	blockBehaviors[BLOCK_WATER_STILL].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos,
	                                                             BlockType /*_blockId*/) -> void {
		// Stack overflow if this was regular set block!
		_world.SetBlockRaw(_pos, BLOCK_WATER_FLOWING, _world.GetMetadata(_pos));
		_world.tickScheduler.ScheduleUpdateTick(_pos, BLOCK_WATER_FLOWING, 5);
	};
	blockBehaviors[BLOCK_WATER_FLOWING].onTick = [](WorldManager& _world, Int3 _pos, uint8_t /*_meta*/,
	                                                Java::Random& _random) -> void {
		FlowingFluidTick(_world, _pos, BLOCK_WATER_FLOWING, BLOCK_WATER_STILL, MaterialType::Water, 5, _random);
	};
}

}; // namespace Blocks