/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/
#include "dispenser.h"
#include "inventory/inventory_interaction.h"
#include "inventory/item_stack.h"
#include <cstddef>

TrapInventoryInteraction::TrapInventoryInteraction(InventoryPlayer* _pinv,
                                                   std::shared_ptr<TileEntityDispenser> _dispenser)
    : InventoryInteraction(&sharedInventory), playerInventory(_pinv), dispenserHandle(_dispenser),
      dispenserInventory(&_dispenser->inventory) {
	sharedInventory.owner = this;
	MergeInventories();
}

TrapInventoryInteraction::~TrapInventoryInteraction() {
	if (!dispenserHandle.expired())
		WriteBack();
}

bool TrapInventoryInteraction::CanExist(PlayerEntity& _player) {
	if (dispenserHandle.expired())
		return false;
	auto locked = dispenserHandle.lock();
	if (!locked)
		return false;
	auto pos = locked->position;
	return (GetDistSquared(_player.position, { pos.x + 0.5, pos.y + 0.5, pos.z + 0.5 }) < 64.0);
}

void TrapInventoryInteraction::InitSnapshot() {
	MergeInventories();
	snapshot = sharedInventory.slots;
}

std::vector<DeltaSlot> TrapInventoryInteraction::TickDiff() {
	MergeInventories();
	return InventoryInteraction::TickDiff();
}

void TrapInventoryInteraction::MergeInventories() {
	size_t slotCount = 0;
	for (auto& slot : dispenserInventory->slots)
		sharedInventory.slots[slotCount++] = slot;
	for (size_t i = 9; i < 45; i++)
		sharedInventory.slots[slotCount++] = playerInventory->slots[i];
}

void TrapInventoryInteraction::WriteBack() {
	bool dispenserChanged = false;
	for (size_t i = 0; i < 9; i++) {
		if (dispenserInventory->slots[i] == sharedInventory.slots[i])
			continue;
		dispenserInventory->slots[i] = sharedInventory.slots[i];
		dispenserChanged = true;
	}
	// Writing slots directly skips OnInventoryChanged, so flag the chest for saving ourselves
	if (dispenserChanged)
		dispenserInventory->OnInventoryChanged();
	for (size_t i = 9; i < 45; i++)
		playerInventory->slots[i - 9 + 9] = sharedInventory.slots[i];
}

void TrapInventoryInteraction::OnShiftClick(int /*_slot*/) {
	// Shift clicking doesn't work on dispensers so no-op
	return;
}