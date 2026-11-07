/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once
#include "tile_entity_base.h"

// Furnace
class TileEntityFurnace : public TileEntity {
private:
	int burnTime = 0;
	int maxBurnTime = 0;
	int cookTime = 0;

public:
	InventoryFurnace inventory;
	using DirtyFlags = uint8_t;

	static constexpr DirtyFlags FLAG_NONE = 0;
	static constexpr DirtyFlags FLAG_BURN_TIME = 1 << 0;
	static constexpr DirtyFlags FLAG_MAX_BURN_TIME = 1 << 1;
	static constexpr DirtyFlags FLAG_COOK_TIME = 1 << 2;

	DirtyFlags dirtyFlags = FLAG_NONE;

	TileEntityFurnace(Int3 _pPosition) : TileEntityFurnace(_pPosition, 0, 0, 0) {};

	TileEntityFurnace(Int3 _pPosition, int _burnTime, int _maxBurnTime, int _cookTime)
	    : TileEntity(TileType::FURNACE, _pPosition), burnTime(_burnTime), maxBurnTime(_maxBurnTime),
	      cookTime(_cookTime) {
		canTick = true;
	};

	Tag Serialize() override;
	void Deserialize(const Tag& _tag) override;
	void Tick(WorldManager& _world) override;

	int GetCookTime() const;
	int GetBurnTime() const;
	int GetMaxBurnTime() const;
	int CalculateFuelTime() const;
};