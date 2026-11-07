/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "blocks.h"
#include "blocks/block_behaviors.h"
#include "blocks/block_properties.h"
#include "constants.h"
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
#include <algorithm>
#include <cmath>

namespace Blocks {

std::vector<Int3> GetBedApproachSpots(WorldManager& _world, Int3 _headPos, Int3 _footPos) {
	std::vector<Int3> spots;
	Cardinal::Value dirs[4] = { Cardinal::Value::North, Cardinal::Value::South, Cardinal::Value::East,
		                         Cardinal::Value::West };
	for (Int3 bedPos : { _headPos, _footPos }) {
		for (auto dir : dirs) {
			Int3 candidate = bedPos.WithOffset(dir);
			if (candidate == _headPos || candidate == _footPos)
				continue;
			if (!_world.InBounds(candidate.y))
				continue;
			if (_world.IsOpenGroundSpot(candidate))
				spots.push_back(candidate);
		}
	}
	return spots;
}

static bool IsValidSpawnBlock(WorldManager& _world, Int3 _pos) {
	return _world.IsBlockNormalCube(_pos.WithOffset(Cardinal::Value::Down)) && !_world.IsBlockNormalCube(_pos) &&
	       !_world.IsBlockNormalCube(_pos.WithOffset(Cardinal::Value::Up)) && !_world.GetMaterial(_pos).isLiquid;
}

bool TriggerNightmareSpawns(WorldManager& _world, PlayerEntity& _player, Int3 _headPos, Int3 _footPos) {
	static constexpr int MAX_SPAWN_ATTEMPTS = 20;
	static constexpr double PATH_END_TOLERANCE = 1.5;

	auto approachSpots = Blocks::GetBedApproachSpots(_world, _headPos, _footPos);
	Pathfinder pathfinder(&_world);

	for (int attempt = 0; attempt < MAX_SPAWN_ATTEMPTS; attempt++) {
		Int3 pos = {
			_headPos.x + (_world.rand.NextInt(32) - _world.rand.NextInt(32)),
			0,
			_headPos.z + (_world.rand.NextInt(32) - _world.rand.NextInt(32)),
		};

		int startY = std::clamp<int>(_headPos.y + (_world.rand.NextInt(16) - _world.rand.NextInt(16)), 1, CHUNK_HEIGHT - 1);

		// Search for an open spot
		int y = startY;
		for (; y > 2 && !_world.IsBlockNormalCube({ pos.x, y - 1, pos.z }); --y) {
		}
		while (y < startY + 16 && _world.InBounds(y) && !IsValidSpawnBlock(_world, { pos.x, y, pos.z }))
			++y;

		if (y >= startY + 16 || !_world.InBounds(y))
			continue;

		pos.y = y;

		std::shared_ptr<HostileEntity> candidate;
		switch (_world.rand.NextInt(3)) {
		case 0:
			candidate = std::make_shared<ZombieEntity>();
			break;
		case 1:
			candidate = std::make_shared<SkeletonEntity>();
			break;
		default:
			candidate = std::make_shared<SpiderEntity>();
			break;
		}
		candidate->world = &_world;
		candidate->entityManager = &_world.entityManager;

		Vec3 spawnPosition = { pos.x + 0.5, double(pos.y), pos.z + 0.5 };
		float rotationYaw = _world.rand.NextFloat() * 360.0f;
		candidate->Teleport(spawnPosition, { rotationYaw, 0.0 });
		if (!candidate->CanSpawnAt())
			continue;

		// Can this one actually reach the sleeping player?
		auto path = pathfinder.FindPath(pos, _headPos, candidate->width, candidate->height, 32.0f);
		if (path.empty())
			continue;

		const Int3& pathEnd = path.back();
		if (std::min(pathEnd.Distance(_headPos), pathEnd.Distance(_footPos)) > PATH_END_TOLERANCE)
			continue;

		_world.entityManager.AddEntity(candidate);

		Int3 teleportTarget = approachSpots.empty()
		                          ? Int3{ pos.x, pos.y + 1, pos.z }
		                          : approachSpots[size_t(_world.rand.NextInt(int(approachSpots.size())))];
		Vec3 teleportPosition = { teleportTarget.x + 0.5, double(teleportTarget.y), teleportTarget.z + 0.5 };
		candidate->Teleport(teleportPosition);

		// Wake the player and don't set their spawn
		_player.WakeUp(/*_confirmSpawn=*/false);

		return true;
	}

	return false;
}

bool TrySpawnNightmare(WorldManager& _world, PlayerEntity& _player) {
	Int3 headPos = _player.bedPosition;

	// The bed might've been broken out from under them while they were dozing off
	if (_world.GetBlockId(headPos) != BLOCK_BED) {
		_player.WakeUp(/*_confirmSpawn=*/false);
		return false;
	}

	auto headMeta = _world.GetMetadata(headPos);
	auto bedDir = GetDirectionFromMeta(BLOCK_BED, headMeta);
	Int3 footPos = headPos.WithOffset(Cardinal::Opposite(bedDir));
	return TriggerNightmareSpawns(_world, _player, headPos, footPos);
}

void RegisterBedBehaviors() {
	blockBehaviors[BlockType::BLOCK_BED] = {
		.getSelectionBox = BedAabb,
		.getRayBounds = BedAabb,
		.getCollider = BedCollider,
	};

	blockBehaviors[BLOCK_BED].onBlockPlaced = [](WorldManager& _world, Int3 _pos, Entity& _placer,
	                                             Cardinal::Value _face, BlockType _blockId, uint8_t /*_meta*/) -> bool {
		// Beds can only be placed by clicking the top face of a block
		if (_face != Cardinal::Value::Up)
			return false;

		// _pos is already the target cell
		const Int3 placePos = _pos;
		if (!_world.InBounds(placePos.y))
			return false;

		const auto dir = Cardinal::FromAngle(_placer.rotationYaw);
		const Int3 headPos = placePos.WithOffset(dir);

		// Is this placement valid?
		if (!_world.IsBlockNormalCube(placePos.WithOffset(Cardinal::Value::Down)) ||
		    !_world.IsBlockNormalCube(headPos.WithOffset(Cardinal::Value::Down)))
			return false;
		if (!IsReplaceable(_world, placePos) || !IsReplaceable(_world, headPos))
			return false;

		uint8_t meta = GetMetaFromDirection(BLOCK_BED, dir);
		_world.SetBlock(placePos, _blockId, meta);
		_world.SetBlock(headPos, _blockId, uint8_t(meta | 0b1000));

		return true;
	};
	//blockBehaviors[BLOCK_BED].onBlockClicked = ToggleDoor;
	blockBehaviors[BLOCK_BED].onBlockDestroyedByPlayer = [](WorldManager& _world, Int3 _pos, Entity& /*_destroyer*/) {
		auto meta = _world.GetMetadata(_pos);
		auto dir = GetDirectionFromMeta(BLOCK_BED, meta);
		if (meta & 0b1000) {
			// We are the head of the bed
			// Offset one down
			_pos.Offset(Cardinal::Opposite(dir));
			if (_world.GetBlockId(_pos) != BLOCK_BED)
				// The foot is not a bed block!
				return;
		}
		// Since we're now guaranteed to be pointing at the foot of the bed,
		// we can continue like this
		Int3 headPos = _pos.WithOffset(dir);
		if (_world.GetBlockId(headPos) == BLOCK_BED && (_world.GetMetadata(headPos) & 0b1000)) {
			_world.SetBlock(headPos, BLOCK_AIR);
		}
		BreakAndDropBlock(_world, _pos);
	};
}

}; // namespace Blocks