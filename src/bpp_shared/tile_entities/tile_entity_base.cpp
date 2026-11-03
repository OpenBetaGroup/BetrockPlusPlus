/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#include "blocks.h"
#include "entities/entity_pig.h"
#include "entities/entity_skeleton.h"
#include "entities/entity_spider.h"
#include "entities/entity_zombie.h"
#include "inventory/inventory.h"
#include "inventory/item_stack.h"
#include "items.h"
#include "items/item_properties.h"
#include "tile_entity.h"
#include "world/world.h"
#include <string>

void TileEntity::Deserialize(const Tag& _tag) {
	// no-op
	return;
}

// TODO: Move all these into separate files just like InventoryInteraction
void TileEntity::Tick(WorldManager& /*_world*/) {
	// no-op
	return;
}

constexpr std::string GetTileNbtId(TileType _type) {
	switch (_type) {
	case TileType::CHEST:
		return "Chest";
	case TileType::DISPENSER:
		return "Trap";
	case TileType::FURNACE:
		return "Furnace";
	case TileType::SIGN:
		return "Sign";
	case TileType::SPAWNER:
		return "MobSpawner";
	case TileType::JUKEBOX:
		return "RecordPlayer";
	case TileType::NOTEBLOCK:
		return "Music";
	case TileType::PISTON_MOVING:
		return "Piston";
	}
	// Invalid type, empty string
	return "";
}

Tag TileEntity::Serialize() {
	auto root = Tag{};
	root.type = TAG_COMPOUND;

	auto id = Tag{ .type = TAG_STRING, .name = "id", .longValue = 0, .stringValue = GetTileNbtId(type) };
	auto x = Tag{ .type = TAG_INT, .name = "x", .intValue = position.x };
	auto y = Tag{ .type = TAG_INT, .name = "y", .intValue = position.y };
	auto z = Tag{ .type = TAG_INT, .name = "z", .intValue = position.z };

	root.compound["id"] = id;
	root.compound["x"] = x;
	root.compound["y"] = y;
	root.compound["z"] = z;

	return root;
}
