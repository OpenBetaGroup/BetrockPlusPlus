/*
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/
#include "crafting.h"
#include "inventory/inventory_interaction.h"
#include "inventory/item_stack.h"
#include "items.h"
#include "numeric_structs.h"

CraftingInventoryInteraction::CraftingInventoryInteraction(Inventory* _sharedInventory, Inventory* _craftingInventory,
                                                           InventoryPlayer* _playerInventory, Runtime& _gameRuntime,
                                                           UInt8_2 _gridSize)
    : InventoryInteraction(_sharedInventory), craftInventory(_craftingInventory), playerInventory(_playerInventory),
      runtime(_gameRuntime), gridSize(_gridSize) {}

void CraftingInventoryInteraction::UpdateResult() {
	auto grid = std::span<const ItemStack>(craftInventory->slots.data() + 1, gridSize.Total());
	auto result = runtime.recipeManager.MatchGrid(grid, gridSize);
	craftInventory->slots[0] = result.second;
	lastRecipe = result.first;
}

void CraftingInventoryInteraction::FinishCraft() {
	craftInventory->slots[0] = ItemStack{};

	// Consume or transform one of each ingredient that went into this craft
	for (size_t i = 1; i <= gridSize.Total(); ++i) {
		const ItemStack transformTo = lastRecipe[i - 1].transformTo;
		if (transformTo.id != Items::Id::INVALID)
			craftInventory->slots[i] = transformTo;
		else
			craftInventory->slots[i].DecrementCount(1);
	}

	// The grid changed, there might be another possible craft
	UpdateResult();
}

void CraftingInventoryInteraction::TakeResult() {
	ItemStack& result = craftInventory->slots[0];
	if (result.id == Items::Id::INVALID)
		return;

	if (carried.id == Items::Id::INVALID) {
		carried = result;
	} else if (carried.id == result.id && carried.data == result.data) {
		// Same type, try merge with cursor
		int maxStack = Items::GetMaxStack(carried.id);
		if (int(carried.count) + int(result.count) > maxStack)
			return;
		carried.count += result.count;
	} else {
		// Cursor holds something else
		return;
	}

	FinishCraft();
}

void CraftingInventoryInteraction::HandleCrafting(int _slot) {
	if (_slot == 0 || _slot > gridSize.Total())
		return;

	UpdateResult();
}

void CraftingInventoryInteraction::OnLeftClick(int _slot) {
	if (_slot == 0)
		TakeResult();
	else
		InventoryInteraction::OnLeftClick(_slot);
	HandleCrafting(_slot);
}

void CraftingInventoryInteraction::OnRightClick(int _slot) {
	if (_slot == 0)
		TakeResult();
	else
		InventoryInteraction::OnRightClick(_slot);
	HandleCrafting(_slot);
}

void CraftingInventoryInteraction::OnShiftClick(int _slot) {
	if (_slot == 0)
		ShiftClickResult();
	else
		ShiftClickOther(_slot);
	HandleCrafting(_slot);
}