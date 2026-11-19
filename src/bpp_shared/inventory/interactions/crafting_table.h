/*
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/
#pragma once
#include "../inventory_interaction.h"
#include "entities/entity_player.h"
#include "inventory/interactions/crafting.h"
#include "inventory/inventories.h"
#include "inventory/inventory.h"
#include "numeric_structs.h"
#include "runtime.h"
#include "world.h"
#include <vector>

struct CraftingTableInventoryInteraction : CraftingInventoryInteraction {
	InventoryCraftingTable craftInventory;
	WorldManager& world;
	Int3 blockPosition;

	struct SharedInventory : Inventory {
		CraftingTableInventoryInteraction* owner = nullptr;
		SharedInventory() : Inventory(46) {}
		void OnInventoryChanged() override {
			if (owner)
				owner->WriteBack();
		}
	} sharedInventory;

	CraftingTableInventoryInteraction(InventoryPlayer* _pinv, WorldManager& _worldMng, Runtime& _gameRuntime,
	                                  Int3 _craftingTablePos);
	~CraftingTableInventoryInteraction();

	bool CanExist(PlayerEntity& player) override;
	void InitSnapshot() override;
	std::vector<DeltaSlot> TickDiff() override;
	void WriteBack();
	void OnInteractionClosed(PlayerEntity& player) override;

protected:
	void MergeInventories();
	void UpdateResult() override;
	void ShiftClickResult() override;
	void ShiftClickOther(int _slot) override;
};