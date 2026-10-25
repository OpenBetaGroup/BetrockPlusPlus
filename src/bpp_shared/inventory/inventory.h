/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/
#pragma once
#include "enums/items.h"
#include "item_stack.h"
#include "items/item_properties.h"
#include <cstdint>
#include <string>
#include <vector>

// Inventory
struct Inventory {
	std::string name = "Inventory";
	std::vector<ItemStack> slots;
	bool isModified = false;

	Inventory(size_t _size) : slots(_size) {}

	virtual int GetSizeInventory() const {
		return int(slots.size());
	}

	virtual NetworkSlotId GetNetworkSlotId(NbtSlotId _slot) const {
		if (_slot < 0 || _slot >= int(slots.size()))
			return -1;
		return _slot.value;
	}

	virtual ItemStack* GetStackInSlot(int _slot) {
		if (_slot < 0 || _slot >= int(slots.size()))
			return nullptr;
		if (slots[size_t(_slot)].id == Items::Id::INVALID)
			return nullptr;
		return &slots[size_t(_slot)];
	}

	virtual ItemStack DecreaseStackSize(int _slot, int _count) {
		if (_slot < 0 || _slot >= int(slots.size()) || slots[size_t(_slot)].id == Items::Id::INVALID)
			return ItemStack{};
		auto& stack = slots[size_t(_slot)];
		if (stack.count <= _count) {
			ItemStack taken = stack;
			slots[_slot] = ItemStack{};
			OnInventoryChanged();
			return taken;
		}
		ItemStack taken{ stack.id, int8_t(_count), stack.data };
		stack.count = int8_t(stack.count - _count);
		OnInventoryChanged();
		return taken;
	}

	virtual void SetInventorySlotContents(int _slot, ItemStack* _stack) {
		if (_slot < 0 || _slot >= int(slots.size()))
			return;
		slots[size_t(_slot)] = _stack ? *_stack : ItemStack{};
		OnInventoryChanged();
	}

	virtual const std::string& GetInventoryName() const {
		return name;
	}
	virtual void OnInventoryChanged() {
		isModified = true;
	}
	virtual ~Inventory() = default;

	void ClearSlot(int _slot) {
		if (_slot < 0 || _slot >= int(slots.size()))
			return;
		slots[size_t(_slot)] = ItemStack{};
		OnInventoryChanged();
	}

	// Returns number of items that were deleted
	size_t Clear() {
		size_t count = 0;
		for (auto& slot : slots) {
			if (Items::IsValidId(slot.id))
				count += slot.count;
			slot = ItemStack{};
		}
		OnInventoryChanged();
		return count;
	}

	// Take in the original item stack, try and merge it with our inventory. Returns if it was successful
	// Start slot and end slot are inclusive
	virtual bool MergeItemStackInInventory(ItemStack& _stack, bool _reverse = false, int _startSlot = 0,
	                                       int _endSlot = -1) {
		auto start = _startSlot;
		auto end = _endSlot == -1 ? GetSizeInventory() - 1 : _endSlot;

		// Try and merge into an already existing stack of the same type if this item is stackable
		if (Items::IsStackable(_stack.id)) {
			for (int i = _reverse ? end : start; _reverse ? i >= start : i <= end; _reverse ? i-- : i++) {
				auto slot = GetStackInSlot(i);
				if (!slot)
					continue;
				if (slot->id == _stack.id && slot->data == _stack.data) {
					auto maxStack = Items::GetMaxStack(slot->id);
					// Don't try and merge into an already maxed out stack
					if (slot->count >= maxStack)
						continue;

					// Add the stacks together and do some checks to make sure we don't overflow
					int space = maxStack - slot->count;
					int toMove = std::min<int>(space, int(_stack.count));

					slot->count += toMove;
					_stack.count -= toMove;

					OnInventoryChanged();
					if (_stack.count == 0)
						return true;
				}
			}
		}

		// We couldn't merge into existing items so just try and find an empty slot
		for (int i = _reverse ? end : start; _reverse ? i >= start : i <= end; _reverse ? i-- : i++) {
			auto slot = GetStackInSlot(i);
			if (!slot) {
				ItemStack newStack = _stack;
				this->SetInventorySlotContents(i, &newStack);
				_stack.id = Items::Id::INVALID;
				_stack.data = 0;
				_stack.count = 0;
				OnInventoryChanged();
				return true;
			}
		}
		return false; // Give up
	}

	// Returns whether we could merge an item stack without changing the inventory.
	virtual bool CanMergeItemStackInInventory(ItemStack& _stack, bool _reverse = false, int _startSlot = 0,
	                                          int _endSlot = -1) {
		auto start = _startSlot;
		auto end = _endSlot == -1 ? GetSizeInventory() - 1 : _endSlot;

		// Try and merge into an already existing stack of the same type if this item is stackable
		if (Items::IsStackable(_stack.id)) {
			for (int i = _reverse ? end : start; _reverse ? i >= start : i <= end; _reverse ? i-- : i++) {
				auto slot = GetStackInSlot(i);
				if (!slot)
					continue;
				if (slot->id == _stack.id && slot->data == _stack.data) {
					auto maxStack = Items::GetMaxStack(slot->id);
					// Don't try and merge into an already maxed out stack
					if (slot->count >= maxStack)
						continue;

					// Add the stacks together and do some checks to make sure we don't overflow
					int space = maxStack - slot->count;
					int toMove = std::min<int>(space, int(_stack.count));

					if (toMove > 0)
						return true;
				}
			}
		}
		return false; // Give up
	}
};
