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

static bool CanPlaceChest(WorldManager& _world, Int3 _pos) {
	// If any of the block surrounding us are double chests we cannot be placed
	Cardinal::Value directions[4] = { Cardinal::Value::East, Cardinal::Value::West, Cardinal::Value::North,
		                              Cardinal::Value::South };

	auto isDoubleChest = [&](Int3 _npos) -> bool {
		if (_world.GetBlockId(_npos) != BLOCK_CHEST)
			return false;
		for (auto& dir : directions) {
			if (_world.GetBlockId(_npos.WithOffset(dir)) == BLOCK_CHEST)
				return true;
		}
		return false;
	};

	for (auto& dir : directions) {
		if (isDoubleChest(_pos.WithOffset(dir)))
			return false;
	}
	return true;
}

bool CanOpenChest(WorldManager& _world, Int3 _pos) {
	auto chest = _world.GetTileEntityShared<TileEntityChest>(_pos);
	if (!chest)
		return false;

	// Is there a block above us?
	if (_world.IsBlockNormalCube(_pos.WithOffset(Cardinal::Value::Up)))
		return false;

	// Check if we are a double chest
	auto l = _world.GetBlockId({ _pos.x - 1, _pos.y, _pos.z });
	auto r = _world.GetBlockId({ _pos.x + 1, _pos.y, _pos.z });
	auto f = _world.GetBlockId({ _pos.x, _pos.y, _pos.z - 1 });
	auto b = _world.GetBlockId({ _pos.x, _pos.y, _pos.z + 1 });
	bool doubleChest = (l == BLOCK_CHEST || r == BLOCK_CHEST || f == BLOCK_CHEST || b == BLOCK_CHEST);

	if (doubleChest) {
		std::shared_ptr<TileEntityChest> partnerChest = nullptr;
		if (l == BLOCK_CHEST)
			partnerChest = _world.GetTileEntityShared<TileEntityChest>({ _pos.x - 1, _pos.y, _pos.z });
		else if (r == BLOCK_CHEST)
			partnerChest = _world.GetTileEntityShared<TileEntityChest>({ _pos.x + 1, _pos.y, _pos.z });
		else if (f == BLOCK_CHEST)
			partnerChest = _world.GetTileEntityShared<TileEntityChest>({ _pos.x, _pos.y, _pos.z - 1 });
		else
			partnerChest = _world.GetTileEntityShared<TileEntityChest>({ _pos.x, _pos.y, _pos.z + 1 });
		if (!partnerChest)
			return false;

		if (_world.IsBlockNormalCube(partnerChest->position.WithOffset(Cardinal::Value::Up)))
			return false;
	}

	return true;
}

void RegisterChestBehaviors() {
	// Chest
	blockBehaviors[BLOCK_CHEST].onBlockPlaced = [](WorldManager& _world, Int3 _pos, Entity& _placer,
	                                               Cardinal::Value _face, BlockType _blockId, uint8_t _meta) -> bool {
		if (!CanPlaceChest(_world, _pos))
			return false;
		return GenericPlace(_world, _pos, _placer, _face, _blockId, _meta);
	};

	blockBehaviors[BLOCK_CHEST].onBlockAdded = [](WorldManager& _world, Int3 _pos) -> void {
		auto chest = std::make_shared<TileEntityChest>(_pos);
		_world.CreateTileEntity(std::move(chest));
	};
	blockBehaviors[BLOCK_CHEST].onBlockRemoval = [](WorldManager& _world, Int3 _pos) -> void {
		auto* te = _world.GetTileEntityAs<TileEntityChest>(_pos);
		if (!te)
			return;

		_world.DropInventory(te->inventory, _pos);
	};
}

}; // namespace Blocks
