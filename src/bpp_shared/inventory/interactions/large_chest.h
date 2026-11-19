/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/
#pragma once
#include "../inventory_interaction.h"
#include "entities/entity_player.h"
#include "inventory/inventories.h"
#include "inventory/inventory.h"
#include "tile_entities/tile_entity_chest.h"
#include <memory>
#include <vector>

struct LargeChestInventoryInteraction : InventoryInteraction {
	InventoryPlayer* playerInventory;
	std::weak_ptr<TileEntityChest> upperChest;
	std::weak_ptr<TileEntityChest> lowerChest;
	InventoryLargeChest chestInventory;

	struct SharedInventory : Inventory {
		LargeChestInventoryInteraction* owner = nullptr;
		SharedInventory() : Inventory(90) {}
		void OnInventoryChanged() override {
			if (owner)
				owner->WriteBack();
		}
	} sharedInventory;

	LargeChestInventoryInteraction(InventoryPlayer* _pinv, std::shared_ptr<TileEntityChest> _upper,
	                               std::shared_ptr<TileEntityChest> _lower);
	virtual ~LargeChestInventoryInteraction();

	bool CanExist(PlayerEntity& player) override;
	void InitSnapshot() override;
	std::vector<DeltaSlot> TickDiff() override;
	void MergeInventories();
	void WriteBack();
	void OnShiftClick(int _slot) override;
};