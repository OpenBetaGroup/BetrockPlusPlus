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

void TryCreatePortal(WorldManager& _world, Int3 _pos) {
	if (_world.GetBlockId(_pos.WithOffset(Cardinal::Value::Down)) != BLOCK_OBSIDIAN)
		return;

	bool isXAligned = (_world.GetBlockId(_pos + Int3{ 1, -1, 0 }) == BLOCK_OBSIDIAN ||
	                   _world.GetBlockId(_pos + Int3{ -1, -1, 0 }) == BLOCK_OBSIDIAN);

	bool isZAligned = (_world.GetBlockId(_pos + Int3{ 0, -1, 1 }) == BLOCK_OBSIDIAN ||
	                   _world.GetBlockId(_pos + Int3{ 0, -1, -1 }) == BLOCK_OBSIDIAN);

	if (!isXAligned && !isZAligned)
		return;

	if (isXAligned && isZAligned)
		return;

	std::array<Int3, 4> neighbors =
	    isXAligned ? std::array<Int3, 4>{ { { 0, 1, 0 }, { 0, -1, 0 }, { 1, 0, 0 }, { -1, 0, 0 } } }
	               : std::array<Int3, 4>{ { { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } } };

	std::vector<Int3> queue = { _pos };
	size_t head = 0;
	constexpr size_t MAX_PORTAL_BLOCKS = 6;

	while (head < queue.size()) {
		Int3 curr = queue[head++];

		if (queue.size() > MAX_PORTAL_BLOCKS)
			return;

		for (const auto& offset : neighbors) {
			Int3 next = curr + offset;
			BlockType id = _world.GetBlockId(next);

			if (id == BLOCK_OBSIDIAN)
				continue;

			if (id != BLOCK_AIR && id != BLOCK_FIRE && id != BLOCK_NETHER_PORTAL)
				return;

			if (std::find(queue.begin(), queue.end(), next) == queue.end()) {
				queue.push_back(next);
			}
		}
	}

	if (queue.empty() || queue.size() != 6)
		return;

	int minX = queue[0].x, maxX = queue[0].x;
	int minY = queue[0].y, maxY = queue[0].y;
	int minZ = queue[0].z, maxZ = queue[0].z;

	for (const auto& block : queue) {
		if (block.x < minX)
			minX = block.x;
		if (block.x > maxX)
			maxX = block.x;
		if (block.y < minY)
			minY = block.y;
		if (block.y > maxY)
			maxY = block.y;
		if (block.z < minZ)
			minZ = block.z;
		if (block.z > maxZ)
			maxZ = block.z;
	}

	int width = isXAligned ? (maxX - minX + 1) : (maxZ - minZ + 1);
	int height = maxY - minY + 1;

	constexpr int MIN_WIDTH = 2;
	constexpr int MIN_HEIGHT = 3;
	constexpr int MAX_WIDTH = 2;
	constexpr int MAX_HEIGHT = 3;

	if (width < MIN_WIDTH || height < MIN_HEIGHT || height > MAX_HEIGHT || width > MAX_WIDTH)
		return;

	for (const auto& innerPos : queue)
		_world.SetBlock(innerPos, BLOCK_NETHER_PORTAL, /*meta=*/0, /*keepTileEntity=*/false, /*notifyNeighbors=*/false);
}

static void GetConnectedPortals(WorldManager& _world, Int3 _pos, std::unordered_set<Int3>& _portals) {
	if (_world.GetBlockId(_pos) != BLOCK_NETHER_PORTAL)
		return;

	bool xAligned = _world.GetBlockId(_pos.WithOffset(Cardinal::Value::West)) == BLOCK_NETHER_PORTAL ||
	                _world.GetBlockId(_pos.WithOffset(Cardinal::Value::East)) == BLOCK_NETHER_PORTAL;
	bool zAligned = _world.GetBlockId(_pos.WithOffset(Cardinal::Value::North)) == BLOCK_NETHER_PORTAL ||
	                _world.GetBlockId(_pos.WithOffset(Cardinal::Value::South)) == BLOCK_NETHER_PORTAL;
	if (xAligned == zAligned)
		return;

	if (!_portals.insert(_pos).second)
		return; // already visited

	auto neighbors = xAligned ? std::array<Cardinal::Value, 4>{ Cardinal::Value::East, Cardinal::Value::West,
		                                                        Cardinal::Value::Up, Cardinal::Value::Down }
	                          : std::array<Cardinal::Value, 4>{ Cardinal::Value::Up, Cardinal::Value::Down,
		                                                        Cardinal::Value::North, Cardinal::Value::South };

	for (auto offset : neighbors)
		GetConnectedPortals(_world, _pos.WithOffset(offset), _portals);
}

void RegisterPortalBehaviors() {
	// Check if this portal block is still valid
	blockBehaviors[BLOCK_NETHER_PORTAL].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos,
	                                                               BlockType /*_blockId*/) -> void {
		BlockType thisBlock = BLOCK_NETHER_PORTAL;
		bool xAligned = _world.GetBlockId(_pos.WithOffset(Cardinal::Value::West)) == thisBlock ||
		                _world.GetBlockId(_pos.WithOffset(Cardinal::Value::East)) == thisBlock;
		bool zAligned = _world.GetBlockId(_pos.WithOffset(Cardinal::Value::North)) == thisBlock ||
		                _world.GetBlockId(_pos.WithOffset(Cardinal::Value::South)) == thisBlock;
		if ((xAligned && zAligned) || (!xAligned && !zAligned)) {
			_world.SetBlock(_pos, BLOCK_AIR);
		}
	};
	blockBehaviors[BLOCK_NETHER_PORTAL].onEntityCollidedWithBlock = [](WorldManager& /*_world*/, Int3 /*_pos*/,
	                                                                   Entity& _entity) -> void {
		if (_entity.passenger.lock() || _entity.vehicle.lock())
			return;
		_entity.OnEnterPortal();
	};

	// Remove the existing portals that depend on this obsidian
	blockBehaviors[BLOCK_OBSIDIAN].onBlockRemoval = [](WorldManager& _world, Int3 _pos) -> void {
		Cardinal::Value neighbors[6] = { Cardinal::Value::East,  Cardinal::Value::West, Cardinal::Value::North,
			                             Cardinal::Value::South, Cardinal::Value::Up,   Cardinal::Value::Down };
		for (auto offset : neighbors) {
			auto checkPos = _pos.WithOffset(offset);
			auto checkBlock = _world.GetBlockId(checkPos);

			if (checkBlock != BLOCK_NETHER_PORTAL)
				continue;

			std::unordered_set<Int3> portalPositions;

			GetConnectedPortals(_world, checkPos, portalPositions);

			for (auto pos : portalPositions) {
				_world.SetBlock(pos, BLOCK_AIR);
			}
		}
	};

	blockBehaviors[BLOCK_NETHER_PORTAL].onBlockRemoval = [](WorldManager& _world, Int3 _pos) -> void {
		Cardinal::Value neighbors[6] = { Cardinal::Value::East,  Cardinal::Value::West, Cardinal::Value::North,
			                             Cardinal::Value::South, Cardinal::Value::Up,   Cardinal::Value::Down };
		for (auto offset : neighbors) {
			auto checkPos = _pos.WithOffset(offset);
			auto checkBlock = _world.GetBlockId(checkPos);

			if (checkBlock != BLOCK_NETHER_PORTAL)
				continue;

			std::unordered_set<Int3> portalPositions;

			GetConnectedPortals(_world, checkPos, portalPositions);

			for (auto pos : portalPositions) {
				_world.SetBlock(pos, BLOCK_AIR);
			}
		}
	};
}

}; // namespace Blocks
