/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "tile_entity_dispenser.h"
#include "tile_entity_base.h"
#include "world/chunk.h"
#include <functional>

void TileEntityDispenser::Deserialize(const Tag& _tag) {
	LoadSlots(_tag, this->inventory.slots);
}

Tag TileEntityDispenser::Serialize() {
	auto root = TileEntity::Serialize();
	SerializeInventory(root, inventory);
	return root;
}

TileEntityDispenser::TileEntityDispenser(Int3 _pPosition) : TileEntity(TileType::DISPENSER, _pPosition) {
	// Mark the chunk dirty on inventory change
	inventory.onModified = [&]() {
		if (chunk) {
			chunk->isModified = true;
			inventory.isModified = false;
		}
	};
};