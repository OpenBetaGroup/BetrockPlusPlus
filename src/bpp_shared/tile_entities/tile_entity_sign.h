/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once
#include "tile_entity_base.h"

// Sign
struct TileEntitySign : TileEntity {
	std::string text1 = "";
	std::string text2 = "";
	std::string text3 = "";
	std::string text4 = "";
	TileEntitySign(Int3 _pPosition) : TileEntity(TileType::SIGN, _pPosition) {};

	Tag Serialize() override;
	void Deserialize(const Tag& _tag) override;
};