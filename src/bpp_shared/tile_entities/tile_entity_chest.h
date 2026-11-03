/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once
#include "tile_entity_base.h"

// Chest
struct TileEntityChest : TileEntity {
	InventoryChest inventory;
	TileEntityChest(Int3 _pPosition) : TileEntity(TileType::CHEST, _pPosition) {};

	Tag Serialize() override;
	void Deserialize(const Tag& _tag) override;
	void Tick(WorldManager& _world) override;
};