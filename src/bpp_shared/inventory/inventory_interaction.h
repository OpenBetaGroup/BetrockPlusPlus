/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/
#pragma once
#include "entities/entity_player.h"
#include "inventory.h"
#include "inventory/item_stack.h"
#include "numeric_structs.h"
#include <vector>

struct DeltaSlot {
	ItemStack stack;
	int slot = 0;
};

// Used for actually interacting with inventories, will typically wrap 1 or more inventory objects for things like chests, etc
struct InventoryInteraction {
	std::vector<ItemStack> snapshot;
	ItemStack carried;
	Inventory* inventory;

	InventoryInteraction(Inventory* _inv);
	double GetDistSquared(Vec3 p1, Vec3 p2) {
		Vec3 delta = p1 - p2;
		return (delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
	}
	virtual ~InventoryInteraction() = default;

	virtual bool CanExist(PlayerEntity& player);

	// Take a snapshot of this inventory
	virtual void InitSnapshot();

	// Analyze the snapshot vs the current inventory
	// Returns a list of slots that are different
	virtual std::vector<DeltaSlot> TickDiff();

	virtual void OnLeftClick(int _slot);

	virtual void OnRightClick(int _slot);

	virtual void OnShiftClick(int _slot);

	virtual void OnInteractionClosed(PlayerEntity& player);
};