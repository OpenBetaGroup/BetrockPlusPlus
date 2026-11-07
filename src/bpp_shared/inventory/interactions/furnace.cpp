/*
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/
#include "furnace.h"
#include "tile_entities/tile_entity.h"

// Furnace tile-entity slot indices: 0 = input, 1 = fuel, 2 = output
static constexpr int RESULT_SLOT = 2;

FurnaceInventoryInteraction::FurnaceInventoryInteraction(InventoryPlayer* _pinv,
                                                         std::shared_ptr<TileEntityFurnace> _tile)
    : InventoryInteraction(&sharedInventory), playerInventory(_pinv), tile(_tile), furnaceInventory(&_tile->inventory) {
	sharedInventory.owner = this;
	MergeInventories();
}

FurnaceInventoryInteraction::~FurnaceInventoryInteraction() {
	if (!tile.expired())
		WriteBack();
}

bool FurnaceInventoryInteraction::CanExist(PlayerEntity& _player) {
	if (tile.expired())
		return false;
	auto locked = tile.lock();
	if (!locked)
		return false;
	auto pos = locked->position;
	return (GetDistSquared(_player.position, { pos.x + 0.5, pos.y + 0.5, pos.z + 0.5 }) < 64.0);
}

void FurnaceInventoryInteraction::InitSnapshot() {
	MergeInventories();
	snapshot = sharedInventory.slots;
}

std::vector<DeltaSlot> FurnaceInventoryInteraction::TickDiff() {
	MergeInventories();
	return InventoryInteraction::TickDiff();
}

void FurnaceInventoryInteraction::MergeInventories() {
	size_t slotCount = 0;
	for (auto& slot : furnaceInventory->slots)
		sharedInventory.slots[slotCount++] = slot;
	for (size_t i = 9; i < 45; i++)
		sharedInventory.slots[slotCount++] = playerInventory->slots[i];
}

void FurnaceInventoryInteraction::WriteBack() {
	bool furnaceChanged = false;
	for (size_t i = 0; i < 3; i++) {
		if (furnaceInventory->slots[i] == sharedInventory.slots[i])
			continue;
		furnaceInventory->slots[i] = sharedInventory.slots[i];
		furnaceChanged = true;
	}
	// Writing slots directly skips OnInventoryChanged, so flag the furnace for saving ourselves
	if (furnaceChanged)
		furnaceInventory->OnInventoryChanged();
	// shared index i (>=3) maps back to playerInventory index i + 6,
	// since MergeInventories() reads playerInventory[9..44] starting at shared index 3.
	for (size_t i = 3; i < 39; i++)
		playerInventory->slots[i + 6] = sharedInventory.slots[i];
}

void FurnaceInventoryInteraction::TakeResult(bool _isRightClick) {
	ItemStack& result = furnaceInventory->slots[RESULT_SLOT];
	if (result.id == Items::Id::INVALID)
		return;

	if (carried.id == Items::Id::INVALID) {
		if (_isRightClick && result.count > 1) {
			// Beta always take the higher of the two if uneven
			int taken = (result.count + 1) / 2;
			int left = result.count - taken;
			result.count = int8_t(left);
			carried = ItemStack{ result.id, int8_t(taken), result.data };
			furnaceInventory->OnInventoryChanged();
			MergeInventories();
			return;
		}
		carried = result;
	} else if (carried.id == result.id && (Items::HasSubtypes(carried.id) ? result.data == carried.data : true)) {
		// Same type, try merge with cursor
		int maxStack = Items::GetMaxStack(carried.id);
		if (int(carried.count) + int(result.count) > maxStack)
			return;
		carried.count += result.count;
	} else {
		// Cursor holds something else
		return;
	}

	// Clear the result slot
	furnaceInventory->slots[RESULT_SLOT] = ItemStack{};
	furnaceInventory->OnInventoryChanged();
	MergeInventories();
}

void FurnaceInventoryInteraction::OnLeftClick(int _slot) {
	if (_slot == RESULT_SLOT) {
		TakeResult();
		return;
	}
	InventoryInteraction::OnLeftClick(_slot);
}

void FurnaceInventoryInteraction::OnRightClick(int _slot) {
	if (_slot == RESULT_SLOT) {
		TakeResult(/*isRightClick=*/true);
		return;
	}
	InventoryInteraction::OnRightClick(_slot);
}

void FurnaceInventoryInteraction::OnShiftClick(int _slot) {
	if (_slot == RESULT_SLOT) {
		ShiftClickResult();
		return;
	}

	auto stack = sharedInventory.GetStackInSlot(_slot);
	if (!stack)
		return;

	ItemStack copy = *stack;

	if (_slot < 3) {
		// Furnace (input/fuel) -> inventory
		// Try the main inventory then the hotbar
		bool success = playerInventory->MergeItemStackInInventory(copy, false, 9, 35);
		if (!success)
			playerInventory->MergeItemStackInInventory(copy, false, 36, 44);
	} else {
		// We can't shift click into the furnace slots themselves, so just try the other area of the inventory
		// We shift clicked in the inventory
		if (_slot >= 3 && _slot < 30) {
			// shared [3,30) == playerInventory main inventory [9,35]
			playerInventory->MergeItemStackInInventory(copy, false, 36, 44);
		} else {
			// shared [30,39) == playerInventory hotbar [36,44]
			playerInventory->MergeItemStackInInventory(copy, false, 9, 35);
		}
	}

	// Update the source in the real inventory before re-merging
	if (_slot < 3) {
		furnaceInventory->slots[size_t(_slot)] = copy.count == 0 ? ItemStack{} : copy;
		furnaceInventory->OnInventoryChanged();
	} else {
		playerInventory->slots[size_t(_slot) + 6] = copy.count == 0 ? ItemStack{} : copy;
	}

	// Re-sync sharedInventory from the real inventories
	MergeInventories();
}

void FurnaceInventoryInteraction::ShiftClickResult() {
	ItemStack& result = furnaceInventory->slots[RESULT_SLOT];
	if (result.id == Items::Id::INVALID)
		return;

	ItemStack copy = result;
	if (playerInventory->MergeItemStackInInventory(copy, true, 9, 44)) {
		// Successfully moved result to inventory
		furnaceInventory->slots[RESULT_SLOT] = ItemStack{};
	} else {
		// Couldn't move all, update with remaining
		furnaceInventory->slots[RESULT_SLOT] = copy.count == 0 ? ItemStack{} : copy;
	}
	furnaceInventory->OnInventoryChanged();
	MergeInventories();
}