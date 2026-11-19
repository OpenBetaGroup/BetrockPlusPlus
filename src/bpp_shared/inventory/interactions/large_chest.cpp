/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/
#include "large_chest.h"
#include "enums/items.h"
#include "inventory/inventory_interaction.h"
#include "inventory/item_stack.h"
#include "numeric_structs.h"
#include <cstddef>

LargeChestInventoryInteraction::LargeChestInventoryInteraction(InventoryPlayer* _pinv,
                                                               std::shared_ptr<TileEntityChest> _upper,
                                                               std::shared_ptr<TileEntityChest> _lower)
    : InventoryInteraction(&sharedInventory), playerInventory(_pinv), upperChest(_upper), lowerChest(_lower),
      chestInventory(&_upper->inventory, &_lower->inventory) {
	sharedInventory.owner = this;
	MergeInventories();
}

LargeChestInventoryInteraction::~LargeChestInventoryInteraction() {
	if (!upperChest.expired() && !lowerChest.expired())
		WriteBack();
}

bool LargeChestInventoryInteraction::CanExist(PlayerEntity& _player) {
	auto lockedUpper = upperChest.lock();
	auto lockedLower = lowerChest.lock();
	if (!lockedUpper || !lockedLower)
		return false;
	auto avgPos = lockedUpper->position + lockedLower->position;
	return (GetDistSquared(_player.position, { avgPos.x / 2.0, avgPos.y / 2.0, avgPos.z / 2.0 }) < 64.0);
}

void LargeChestInventoryInteraction::InitSnapshot() {
	MergeInventories();
	snapshot = sharedInventory.slots;
}

std::vector<DeltaSlot> LargeChestInventoryInteraction::TickDiff() {
	MergeInventories();
	return InventoryInteraction::TickDiff();
}

void LargeChestInventoryInteraction::MergeInventories() {
	size_t slotCount = 0;
	for (size_t i = 0; i < size_t(chestInventory.GetSizeInventory()); i++) {
		auto* stack = chestInventory.GetStackInSlot(i);
		sharedInventory.slots[slotCount++] = stack ? *stack : ItemStack{};
	}
	for (size_t i = 9; i < 45; i++)
		sharedInventory.slots[slotCount++] = playerInventory->slots[i];
}

void LargeChestInventoryInteraction::WriteBack() {
	for (size_t i = 0; i < 54; i++) {
		auto& slot = sharedInventory.slots[i];
		ItemStack* ptr = slot.id != Items::Id::INVALID ? &slot : nullptr;
		chestInventory.SetInventorySlotContents(i, ptr);
	}
	for (size_t i = 54; i < 90; i++)
		playerInventory->slots[i - 54 + 9] = sharedInventory.slots[i];
}

void LargeChestInventoryInteraction::OnShiftClick(int _slot) {
	auto stack = sharedInventory.GetStackInSlot(_slot);
	if (!stack)
		return;

	ItemStack copy = *stack;

	if (_slot <= 53) {
		// Chest -> inventory
		playerInventory->MergeItemStackInInventory(copy, true, 9, 44);
	} else {
		// Inventory -> Chest
		chestInventory.MergeItemStackInInventory(copy);
	}

	// Update the source in the real inventory before re-merging
	if (_slot <= 53) {
		ItemStack* ptr = copy.count == 0 ? nullptr : &copy;
		chestInventory.SetInventorySlotContents(_slot, ptr);
	} else {
		int playerSlot = _slot - 54 + 9;
		playerInventory->slots[size_t(playerSlot)] = copy.count == 0 ? ItemStack{} : copy;
	}

	// Re-sync sharedInventory from the real inventories
	MergeInventories();
}