/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/
#include "player.h"
#include "../../entities/entity_player.h"
#include "inventory/interactions/crafting.h"
#include "inventory/inventories.h"

PlayerInventoryInteraction::PlayerInventoryInteraction(InventoryPlayer* _inv, Runtime& _gameRuntime)
    : CraftingInventoryInteraction(_inv, _inv, _inv, _gameRuntime, { 2, 2 }), playerInventory(_inv) {}

void PlayerInventoryInteraction::OnInteractionClosed(PlayerEntity& player) {
	// Drop the crafting grid items on inventory close
	for (size_t i = 1; i <= 4; i++) {
		ItemStack& stack = inventory->slots[i];
		player.DropItem(stack);
		stack = ItemStack{};
	}
	// Drop the cursor
	if (carried.id != Items::INVALID) {
		player.DropItem(carried);
		carried = ItemStack{};
	}
}

void PlayerInventoryInteraction::OnLeftClick(int _slot) {
	auto playerInv = dynamic_cast<InventoryPlayer*>(inventory);
	if (!playerInv)
		return;

	if (playerInv->GetInventoryAreaFromSlot(_slot) == InvMap::ARMOR) {
		// Slots 5 -> 8 are for armor
		if (this->carried.id != Items::Id::INVALID) {
			// Check to see if the armor piece matches
			if (!Items::IsArmor(this->carried.id))
				return;

			Items::ArmorPiece pieces[4] = { Items::ArmorPiece::HELMET, Items::ArmorPiece::CHESTPLATE,
				                            Items::ArmorPiece::LEGGING, Items::ArmorPiece::BOOT };

			if (pieces[_slot - 5] != Items::GetArmorPiece(this->carried.id))
				return;
		}
	}

	CraftingInventoryInteraction::OnLeftClick(_slot);
}

void PlayerInventoryInteraction::OnRightClick(int _slot) {
	auto playerInv = dynamic_cast<InventoryPlayer*>(inventory);
	if (!playerInv)
		return;

	if (playerInv->GetInventoryAreaFromSlot(_slot) == InvMap::ARMOR) {
		// Slots 5 -> 8 are for armor
		if (this->carried.id != Items::Id::INVALID) {
			// Check to see if the armor piece matches
			if (!Items::IsArmor(this->carried.id))
				return;

			Items::ArmorPiece pieces[4] = { Items::ArmorPiece::HELMET, Items::ArmorPiece::CHESTPLATE,
				                            Items::ArmorPiece::LEGGING, Items::ArmorPiece::BOOT };

			if (pieces[_slot - 5] != Items::GetArmorPiece(this->carried.id))
				return;
		}
	}

	CraftingInventoryInteraction::OnRightClick(_slot);
}

bool PlayerInventoryInteraction::CanExist(PlayerEntity& /*_player*/) {
	return playerInventory != nullptr;
}

void PlayerInventoryInteraction::ShiftClickResult() {
	ItemStack& result = playerInventory->slots[0];
	if (result.id == Items::Id::INVALID)
		return;

	ItemStack copy = result;
	if (playerInventory->MergeItemStackInInventory(copy, true, 9, 44)) {
		FinishCraft();
	} else {
		playerInventory->slots[0] = copy.count == 0 ? ItemStack{} : copy;
	}
}

void PlayerInventoryInteraction::ShiftClickOther(int _slot) {
	auto from = playerInventory->GetInventoryAreaFromSlot(_slot);
	auto stack = playerInventory->GetStackInSlot(_slot);
	if (!stack)
		return;

	ItemStack copy = *stack; // work on a copy so we can detect what moved

	if (from == InvMap::ARMOR || from == InvMap::CRAFTING_AREA || from == InvMap::CRAFTING_RESULT ||
	    from == InvMap::HOTBAR) {
		bool success = playerInventory->MergeItemStackInInventory(copy, false, 9, 35);
		if (!success && from != InvMap::HOTBAR)
			playerInventory->MergeItemStackInInventory(copy, false, 36, 44);
	} else {
		playerInventory->MergeItemStackInInventory(copy, false, 36, 44);
	}

	// Update the source slot to whatever count is left
	if (copy.count == 0) {
		playerInventory->ClearSlot(_slot);
	} else {
		stack->count = copy.count;
	}
}