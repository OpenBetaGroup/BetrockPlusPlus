/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once

#include "../numeric_structs.h"
#include <cstdint>
#include <map>
#include <vector>
#include "blocks/materials.h"

namespace Map {
enum class Icon : uint8_t {
	WhiteArrow,
	GreenArrow,
	RedArrow,
	BlueArrow,
	WhiteCross
};

struct MarkerPlacement {
	Icon icon;
	uint8_t rotation;
	Byte2 position;
};
MarkerPlacement MakeMarker(const Icon _icon, const float _rotation, const Byte2 _position);
void AppendPixel(std::vector<uint8_t>& _mapData, const uint8_t _value);
void InitGraphics(std::vector<uint8_t>& _mapData, const Byte2 _offset);
void PlaceMarkers(std::vector<uint8_t>& _mapData, const std::vector<MarkerPlacement> _markers);

void InitMaps();
struct Item {
	static constexpr uint8_t MAP_ITEM_WIDTH = 128;
	static constexpr uint16_t MAP_ITEM_AREA = MAP_ITEM_WIDTH * MAP_ITEM_WIDTH;
	// Map image data
	// TODO: Could THEORETICALLY be backed into a 5-bit array (max of 27 materials)
	std::array<MaterialType, MAP_ITEM_AREA> data;
	// Map origin (where it was crafted)
	Int32_2 origin = {0,0};
	// Map scale (max 0,1,2,3,4, aka 1,2,4,8,16)
	int8_t scale : 2;
	// If anything on the map has changed and updates need to be sent out
	bool dirty : 1;
	void Init(const Int32_2 _origin);
	void Tick();
};

extern std::map<int16_t, Map::Item> mapItems;
}; // namespace Map