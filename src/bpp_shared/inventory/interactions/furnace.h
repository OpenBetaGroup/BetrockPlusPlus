/*
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/
#pragma once
#include "../inventory_interaction.h"
#include "inventory/inventories.h"
#include "tile_entities/tile_entity.h"

struct FurnaceInventoryInteraction : InventoryInteraction {
	InventoryPlayer* playerInventory;
	std::weak_ptr<TileEntityFurnace> tile;
	InventoryFurnace* furnaceInventory;

	struct SharedInventory : Inventory {
		FurnaceInventoryInteraction* owner = nullptr;
		SharedInventory() : Inventory(39) {}
		void OnInventoryChanged() override {
			if (owner)
				owner->WriteBack();
		}
	} sharedInventory;

	FurnaceInventoryInteraction(InventoryPlayer* _pinv, std::shared_ptr<TileEntityFurnace> _tile);
	virtual ~FurnaceInventoryInteraction();

	virtual bool CanExist(PlayerEntity& player) override;
	void InitSnapshot() override;
	std::vector<DeltaSlot> TickDiff() override;
	void MergeInventories();
	void WriteBack();
	void OnLeftClick(int _slot) override;
	void OnRightClick(int _slot) override;
	void OnShiftClick(int _slot) override;
	void ShiftClickResult();
	void TakeResult(bool _isRightClick = false);
};