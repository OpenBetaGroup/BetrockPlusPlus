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
#include "tile_entities/tile_entity_dispenser.h"
#include <memory>
#include <vector>

struct TrapInventoryInteraction : InventoryInteraction {
	InventoryPlayer* playerInventory = nullptr;
	std::weak_ptr<TileEntityDispenser> dispenserHandle;
	InventoryDispenser* dispenserInventory = nullptr;

	struct SharedInventory : Inventory {
		TrapInventoryInteraction* owner = nullptr;
		SharedInventory() : Inventory(45) {}
		void OnInventoryChanged() override {
			if (owner)
				owner->WriteBack();
		}
	} sharedInventory;

	TrapInventoryInteraction(InventoryPlayer* _pinv, std::shared_ptr<TileEntityDispenser> _dispenser);
	virtual ~TrapInventoryInteraction();

	virtual bool CanExist(PlayerEntity& player) override;
	void InitSnapshot() override;
	std::vector<DeltaSlot> TickDiff() override;
	void MergeInventories();
	void WriteBack();
	void OnShiftClick(int _slot) override;
};