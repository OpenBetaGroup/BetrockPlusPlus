/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/
#include "inventory_interaction.h"

InventoryInteraction::InventoryInteraction(Inventory* _inv) : inventory(_inv) {}

bool InventoryInteraction::CanExist(PlayerEntity& /*_player*/) {
	return inventory != nullptr;
}

void InventoryInteraction::InitSnapshot() {
	snapshot = inventory->slots;
}

std::vector<DeltaSlot> InventoryInteraction::TickDiff() {
	std::vector<DeltaSlot> differences;
	for (size_t i = 0; i < snapshot.size(); i++) {
		inventory->GetStackInSlot(i);
		auto& snap = snapshot[i];

		bool changed = snap != inventory->slots[i];
		if (!changed)
			continue;

		snap = inventory->slots[i];
		differences.push_back({ snap, int(i) });
	}
	return differences;
}

void InventoryInteraction::OnLeftClick(int _slot) {
	auto targetSlot = inventory->GetStackInSlot(_slot);

	// Empty slot
	if (!targetSlot) {
		if (carried.id != Items::Id::INVALID) {
			inventory->SetInventorySlotContents(_slot, &carried);
			carried = ItemStack{};
		}
		inventory->OnInventoryChanged();
		return;
	}

	// Not carrying anything
	if (carried.id == Items::Id::INVALID) {
		carried = *targetSlot;
		inventory->ClearSlot(_slot);
		inventory->OnInventoryChanged();
		return;
	}

	// Same item; merge
	if (targetSlot->id == carried.id && (Items::HasSubtypes(carried.id) ? targetSlot->data == carried.data : true)) {
		int maxStack = Items::GetMaxStack(targetSlot->id);
		int space = maxStack - targetSlot->count;
		int toMove = std::min(space, int(carried.count));
		targetSlot->count += toMove;
		carried.count -= toMove;
		if (carried.count == 0)
			carried = ItemStack{};
		inventory->OnInventoryChanged();
		return;
	}

	// Different item; swap
	ItemStack temp = *targetSlot;
	*targetSlot = carried;
	carried = temp;
	inventory->OnInventoryChanged();
}

void InventoryInteraction::OnRightClick(int _slot) {
	auto targetSlot = inventory->GetStackInSlot(_slot);

	if (carried.id != Items::Id::INVALID && carried.count > 0) {
		if (!targetSlot) {
			ItemStack single{ carried.id, 1, carried.data };
			inventory->SetInventorySlotContents(_slot, &single);
			carried.count -= 1;
			if (carried.count == 0)
				carried = ItemStack{};
			inventory->OnInventoryChanged();
			return;
		}

		// If we right click on the same item we are carrying just add one
		if (targetSlot->id == carried.id && (Items::HasSubtypes(carried.id) ? targetSlot->data == carried.data : true)) {
			int maxStack = Items::GetMaxStack(targetSlot->id);
			int space = maxStack - targetSlot->count;
			if (space >= 1) {
				targetSlot->count += 1;
				carried.count -= 1;
				if (carried.count == 0)
					carried = ItemStack{};
				inventory->OnInventoryChanged();
			}
			return;
		}

		// If we right click on a different item, swap the cursor and that item
		ItemStack temp = *targetSlot;
		*targetSlot = carried;
		carried = temp;
		inventory->OnInventoryChanged();
		return;
	}

	if (!targetSlot)
		return;

	// Only split items if there stack count is greater than 1 and we aren't carrying anything
	if (targetSlot->count > 1) {
		// Beta always take the higher of the two if uneven
		int taken = (targetSlot->count + 1) / 2;
		int left = targetSlot->count - taken;
		targetSlot->count = int8_t(left);
		carried = ItemStack{ targetSlot->id, int8_t(taken), targetSlot->data };
		inventory->OnInventoryChanged();
		return;
	}

	// If its only one item we just pick it up
	carried = *targetSlot;
	inventory->ClearSlot(_slot);
	inventory->OnInventoryChanged();
	return;
}

void InventoryInteraction::OnShiftClick(int _slot) {
	auto targetSlot = inventory->GetStackInSlot(_slot);
	if (!targetSlot)
		return;
	inventory->MergeItemStackInInventory(*targetSlot);
}

void InventoryInteraction::OnInteractionClosed(PlayerEntity& _player) {
	_player.DropItem(this->carried);
	this->carried = {};
}
