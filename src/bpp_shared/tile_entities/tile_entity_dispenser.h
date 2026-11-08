/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once
#include "tile_entity_base.h"
#include "java_random.h"

// Dispenser (Trap)
struct TileEntityDispenser : TileEntity {
	Java::Random rand;
	InventoryDispenser inventory;
	TileEntityDispenser(Int3 _pPosition) : TileEntity(TileType::DISPENSER, _pPosition) {};

	Tag Serialize() override;
	void Deserialize(const Tag& _tag) override;
	void Tick(WorldManager& _world) override;

	std::optional<ItemStack*> GetRandomStackInInventory() {
		int chosen = -1;
		int seen = 1;

		// Only non empty slots affect distribution
		for (int i = 0; i < inventory.GetSizeInventory(); i++) {
			if (inventory.GetStackInSlot(i) != nullptr && rand.NextInt(seen++) == 0)
				chosen = i;
		}

		if (chosen >= 0)
			return inventory.GetStackInSlot(chosen);
		return std::nullopt;
	}
};