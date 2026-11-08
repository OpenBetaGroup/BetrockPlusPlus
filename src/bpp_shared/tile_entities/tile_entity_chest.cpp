/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "tile_entity_chest.h"
#include "world/chunk.h"

void TileEntityChest::Deserialize(const Tag& _tag) {
	LoadSlots(_tag, this->inventory.slots);
}

void TileEntityChest::Tick(WorldManager& /*_world*/) {
	if (chunk && inventory.isModified) {
		chunk->isModified = true;
		inventory.isModified = false;
	}
}

Tag TileEntityChest::Serialize() {
	auto root = TileEntity::Serialize();
	SerializeInventory(root, inventory);
	return root;
}