/*
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/
#include "crafting_table.h"
#include "blocks.h"
#include "inventory/interactions/crafting.h"
#include "inventory/item_stack.h"
#include "items.h"

CraftingTableInventoryInteraction::CraftingTableInventoryInteraction(InventoryPlayer* _pinv, WorldManager& _worldMng,
                                                                     Runtime& _gameRuntime, Int3 _craftingTablePos)
    : CraftingInventoryInteraction(&sharedInventory, &craftInventory, _pinv, _gameRuntime, { 3, 3 }), world(_worldMng),
      blockPosition(_craftingTablePos) {
	sharedInventory.owner = this;
	MergeInventories();
}

CraftingTableInventoryInteraction::~CraftingTableInventoryInteraction() {
	WriteBack();
}

void CraftingTableInventoryInteraction::WriteBack() {
	size_t slotCount = 0;
	for (size_t i = 0; i < 10; i++)
		craftInventory.slots[i] = sharedInventory.slots[slotCount++];
	for (size_t i = 9; i < 45; i++)
		playerInventory->slots[i] = sharedInventory.slots[slotCount++];
}

bool CraftingTableInventoryInteraction::CanExist(PlayerEntity& _player) {
	return world.GetBlockId(blockPosition) == BLOCK_CRAFTING_TABLE &&
	       (GetDistSquared(_player.position, { blockPosition.x + 0.5, blockPosition.y + 0.5, blockPosition.z + 0.5 }) <
	        64.0);
}

void CraftingTableInventoryInteraction::OnInteractionClosed(PlayerEntity& _player) {
	// Drop cursor
	_player.DropItem(this->carried);
	this->carried = {};

	// Drop our inventory
	// Don't drop the result slot
	for (int i = 1; i < 10; i++) {
		auto& stack = this->craftInventory.slots[i];
		_player.DropItem(stack);
		stack = {};
	}
}

void CraftingTableInventoryInteraction::InitSnapshot() {
	snapshot.resize(size_t(sharedInventory.GetSizeInventory()));
	for (size_t i = 0; i < snapshot.size(); i++)
		snapshot[i] = sharedInventory.slots[i];
}

std::vector<DeltaSlot> CraftingTableInventoryInteraction::TickDiff() {
	std::vector<DeltaSlot> differences;
	MergeInventories(); // make sure sharedInventory is current before diffing
	for (size_t i = 0; i < snapshot.size(); i++) {
		auto& snap = snapshot[i];
		if (snap == sharedInventory.slots[i])
			continue;
		snap = sharedInventory.slots[i];
		differences.push_back({ snap, int(i) });
	}
	return differences;
}

void CraftingTableInventoryInteraction::MergeInventories() {
	size_t slotCount = 0;
	for (size_t i = 0; i < 10; i++)
		sharedInventory.slots[slotCount++] = craftInventory.slots[i];
	for (size_t i = 9; i < 45; i++)
		sharedInventory.slots[slotCount++] = playerInventory->slots[i];
}

void CraftingTableInventoryInteraction::UpdateResult() {
	CraftingInventoryInteraction::UpdateResult();
	MergeInventories();
}

void CraftingTableInventoryInteraction::ShiftClickResult() {
	ItemStack& result = craftInventory.slots[0];
	if (result.id == Items::Id::INVALID)
		return;

	ItemStack copy = result;
	playerInventory->MergeItemStackInInventory(copy, true, 9, 44);
	if (copy.count != result.count) {
		FinishCraft();
	} else {
		craftInventory.slots[0] = copy.count == 0 ? ItemStack{} : copy;
	}
}

void CraftingTableInventoryInteraction::ShiftClickOther(int _slot) {
	auto stack = sharedInventory.GetStackInSlot(_slot);
	if (!stack)
		return;

	ItemStack copy = *stack;

	if (_slot < 10) {
		// Grid -> inventory
		// Try the main inventory then the hotbar
		bool success = playerInventory->MergeItemStackInInventory(copy, false, 9, 35);
		if (!success)
			playerInventory->MergeItemStackInInventory(copy, false, 36, 44);
	} else {
		// We can't shift click into the crafting grid itself, so just try the other area of the inventory
		// We shift clicked in the inventory
		if (_slot > 9 && _slot < 37) {
			playerInventory->MergeItemStackInInventory(copy, false, 36, 44);
		} else {
			playerInventory->MergeItemStackInInventory(copy, false, 9, 35);
		}
	}

	// Update the source in the real inventory before re-merging
	if (_slot < 10) {
		craftInventory.slots[_slot] = copy.count == 0 ? ItemStack{} : copy;
	} else {
		playerInventory->slots[_slot - 1] = copy.count == 0 ? ItemStack{} : copy;
	}

	// Re-sync sharedInventory from the real inventories
	MergeInventories();
}