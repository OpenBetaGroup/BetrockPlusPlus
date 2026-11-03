/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once
#include "tile_entity_base.h"

struct TileEntityNoteblock : TileEntity {
	int8_t note = 0;
	TileEntityNoteblock(Int3 _pPosition) : TileEntity(TileType::NOTEBLOCK, _pPosition) {};

	Tag Serialize() override;
	void Deserialize(const Tag& _tag) override;
};