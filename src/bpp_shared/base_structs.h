/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 * 
*/

#pragma once
#include "blocks.h"
#include <cstdint>
#include <sstream>
#include <string>

// Block Struct
struct Block {
	BlockType type = BLOCK_AIR;
	uint8_t data = 0;

	friend std::ostream& operator<<(std::ostream& _os, const Block& _b) {
		_os << "(" << int32_t(_b.type) << ":" << int32_t(_b.data) << ")";
		return _os;
	}

	std::string Str() const {
		std::ostringstream oss;
		oss << *this; // Use the overloaded << operator
		return oss.str();
	}
};

// Lighting + Block Struct
struct LitBlock : Block {
	uint8_t blocklight : 4 = 0;
	uint8_t skylight : 4 = 0;
};
