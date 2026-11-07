/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#pragma once
#include "enums/items.h"
#include "events.h"
#include "inventory.h"
#include "item_stack.h"
#include "logger.h"
#include <algorithm>
#include <memory>
#include <optional>
#include <random>

enum class InvMap : uint8_t {
	ARMOR,
	INVENTORY,
	HOTBAR,
	CRAFTING_AREA,
	CRAFTING_RESULT,
	INVALID
};

// Network format (The rest of the inventories are self explanatory this is the only one that is semi-convoluted):
// Slots 5 -> 8 are for armor
// Slots 36 -> 44 are the hotbar
// Slots 9 -> 35 are the main inventory
// Slots 1 -> 4 are the crafting grid
// Slot 0 is the crafting result
struct InventoryPlayer : Inventory {
public:
	int activeHotbarSlot = 0;
	EventEmitter<ItemStack*> heldItemUpdate;
	int currentItem = 0;

	InventoryPlayer() : Inventory(45) {
		name = "Inventory";
	}

	void SetHeldItem(ItemStack* _stack) {
		heldItemUpdate.Call(_stack);
	}

	ItemStack* GetCurrentItem() {
		if (currentItem < 0 || currentItem >= 9)
			return nullptr;
		return GetStackInSlot(currentItem);
	}

	ItemStack* GetHeldItem() {
		if (activeHotbarSlot < 0 || activeHotbarSlot >= 9)
			return nullptr;
		return GetStackInSlot(activeHotbarSlot + 36);
	}

	InvMap GetInventoryAreaFromSlot(int _slot) {
		if (_slot == 0)
			return InvMap::CRAFTING_RESULT;
		if (_slot >= 1 && _slot <= 4)
			return InvMap::CRAFTING_AREA;
		if (_slot >= 5 && _slot <= 8)
			return InvMap::ARMOR;
		if (_slot >= 36 && _slot <= 44)
			return InvMap::HOTBAR;
		if (_slot >= 9 && _slot <= 35)
			return InvMap::INVENTORY;
		Logger::Error << "Invalid Inventory area slot! (" << _slot << ")\n";
		return InvMap::INVALID; // Fallback,
	}

	NbtSlotId GetNbtSlotId(NetworkSlotId _slot) {
		if (_slot >= 9 && _slot <= 35)
			return _slot.value;
		if (_slot >= 5 && _slot <= 8)
			return (5 + (8 - _slot)) + 95;
		if (_slot >= 36 && _slot <= 44)
			return _slot - 36;
		return -1;
	}

	NetworkSlotId GetNetworkSlotId(NbtSlotId _slot) const override {
		if (_slot >= 100 && _slot <= 103)
			return 5 + (8 - (_slot - 95));
		if (_slot >= 9 && _slot <= 35)
			return _slot.value;
		if (_slot >= 0 && _slot <= 8)
			return _slot + 36;
		return -1;
	}

	// Tries to "pickup" an item. Returns if it succeeded
	// Not sure why vanilla does it in such a convoluted way
	// but it is the way it is
	bool PickupItem(ItemStack& _stack) {
		// Can we combine with anything in the inventory?
		if (CanMergeItemStackInInventory(_stack, false, 9, 35)) {
			MergeItemStackInInventory(_stack, false, 9, 35);
			return true;
		} else {
			// We couldn't combine this stack with anything in the inventory
			// so try the hotbar
			if (MergeItemStackInInventory(_stack, false, 36, 44)) {
				return true;
			}
			// Try to find an empty slot in the inventory as a last resort
			return MergeItemStackInInventory(_stack, false, 9, 35);
		}
	}
};

struct InventoryChest : Inventory {
	InventoryChest() : Inventory(27) {
		name = "Chest";
	}
};

// Just a wrapper for two chest inventories
struct InventoryLargeChest : Inventory {
	InventoryChest* upper;
	InventoryChest* lower;

	InventoryLargeChest(InventoryChest* _upper, InventoryChest* _lower) : Inventory(0), upper(_upper), lower(_lower) {
		name = "Large Chest";
	}

	int GetSizeInventory() const override {
		return upper->GetSizeInventory() + lower->GetSizeInventory();
	}

	ItemStack* GetStackInSlot(int _slot) override {
		int upperSize = upper->GetSizeInventory();
		if (_slot < upperSize)
			return upper->GetStackInSlot(_slot);
		return lower->GetStackInSlot(_slot - upperSize);
	}

	ItemStack DecreaseStackSize(int _slot, int _count) override {
		int upperSize = upper->GetSizeInventory();
		if (_slot < upperSize)
			return upper->DecreaseStackSize(_slot, _count);
		return lower->DecreaseStackSize(_slot - upperSize, _count);
	}

	void SetInventorySlotContents(int _slot, ItemStack* _stack) override {
		int upperSize = upper->GetSizeInventory();
		if (_slot < upperSize)
			upper->SetInventorySlotContents(_slot, _stack);
		else
			lower->SetInventorySlotContents(_slot - upperSize, _stack);
	}

	void OnInventoryChanged() override {
		upper->OnInventoryChanged();
		lower->OnInventoryChanged();
	}
};

struct InventoryCraftingTable : Inventory {
	InventoryCraftingTable() : Inventory(10) {
		name = "Crafting";
	}
};

struct InventoryDispenser : Inventory {
	InventoryDispenser() : Inventory(9) {
		name = "Trap";
	}
};

// TODO: Maybe make an enum for this?
// Slots: 0 = input, 1 = fuel, 2 = output.
struct InventoryFurnace : Inventory {
	InventoryFurnace() : Inventory(3) {
		name = "Furnace";
	}
};