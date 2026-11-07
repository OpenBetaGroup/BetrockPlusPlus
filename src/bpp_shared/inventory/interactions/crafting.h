/*
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/
#pragma once
#include "../inventory_interaction.h"
#include "inventory/inventories.h"
#include "inventory/inventory.h"
#include "runtime.h"

struct CraftingInventoryInteraction : InventoryInteraction {
private:
	Inventory* craftInventory;

public:
	InventoryPlayer* playerInventory;
	Runtime& runtime;

	// We don't do any inventory merging in this class because
	// sharedInventory, craftingInventory, playerInventory all may or may not be the same thing.
	CraftingInventoryInteraction(Inventory* _sharedInventory, Inventory* _craftingInventory,
	                             InventoryPlayer* _playerInventory, Runtime& _gameRuntime, UInt8_2 _gridSize);

	void OnLeftClick(int _slot) override;
	void OnRightClick(int _slot) override;
	void OnShiftClick(int _slot) override;

protected:
	const UInt8_2 gridSize;
	std::array<ItemKey, 9> lastRecipe;

	void HandleCrafting(int _slot);
	void FinishCraft();
	void TakeResult();

	virtual void UpdateResult();
	virtual void ShiftClickResult() = 0;
	virtual void ShiftClickOther(int _slot) = 0;
};