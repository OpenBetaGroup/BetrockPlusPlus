/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/
#include "player.h"
#include "../../entities/entity_player.h"
#include "enums/items.h"
#include "inventory/interactions/crafting.h"
#include "inventory/inventories.h"
#include "inventory/inventory.h"
#include "inventory/item_stack.h"
#include "items/item_properties.h"
#include <cstddef>
#include <vector>

PlayerInventoryInteraction::PlayerInventoryInteraction(InventoryPlayer* _inv, Runtime& _gameRuntime)
    : CraftingInventoryInteraction(_inv, _inv, _inv, _gameRuntime, { 2, 2 }), playerInventory(_inv) {}

void PlayerInventoryInteraction::OnInteractionClosed(PlayerEntity& _player) {
	// Drop the crafting grid items on inventory close
	for (size_t i = 1; i <= 4; i++) {
		ItemStack& stack = inventory->slots[i];
		_player.DropItem(stack);
		stack = ItemStack{};
	}
	// Drop the cursor
	if (carried.id != Items::INVALID) {
		_player.DropItem(carried);
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

			// We are a msc block
			InventoryInteraction::OnRightClick(_slot);
			return;
		}
	}

	// Thanks pumpkins for making my life hard
	if (_slot == 0)
		TakeResult();
	else {
		InventoryInteraction::OnLeftClick(_slot);
	}
	HandleCrafting(_slot);
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

	if (_slot == 0)
		TakeResult();
	else {
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
			if (targetSlot->id == carried.id &&
			    (Items::HasSubtypes(carried.id) ? targetSlot->data == carried.data : true)) {
				int maxStack = playerInv->GetInventoryAreaFromSlot(_slot) != InvMap::ARMOR
				                   ? Items::GetMaxStack(targetSlot->id)
				                   : 1;
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
	HandleCrafting(_slot);
}

bool PlayerInventoryInteraction::CanExist(PlayerEntity& /*_player*/) {
	return playerInventory != nullptr;
}

void PlayerInventoryInteraction::ShiftClickResult() {
	ItemStack& result = playerInventory->slots[0];
	if (result.id == Items::Id::INVALID)
		return;

	ItemStack copy = result;
	playerInventory->MergeItemStackInInventory(copy, true, 9, 44);
	if (copy.count != result.count) {
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

	ItemStack copy = *stack;

	if (from == InvMap::INVENTORY)
		playerInventory->MergeItemStackInInventory(copy, false, 36, 44);
	else if (from == InvMap::HOTBAR)
		playerInventory->MergeItemStackInInventory(copy, false, 9, 35);
	else // crafting grid, armour
		playerInventory->MergeItemStackInInventory(copy, false, 9, 44);

	if (copy.count == 0)
		playerInventory->ClearSlot(_slot);
	else
		stack->count = copy.count;
}