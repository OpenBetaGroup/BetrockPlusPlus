/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once

#include "../numeric_structs.h"
#include <array>
#include <cstdint>
#include <map>
#include <optional>
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
// The map data length is written as a single unsigned byte in the packet (vanilla reads it with & 0xFF)
static constexpr size_t MAX_PACKET_DATA = UINT8_MAX;

// Two different coordinate spaces are in play here:
//  - Pixels (the graphics data) are relative to the map's top-left corner, 0 to MAP_ITEM_WIDTH-1.
//  - Markers (the icons) are relative to the map's CENTRE, in half-pixels, -128 to 127.
// Each pixel covers (1 << scale) blocks.

// Snaps a world position to the centre that a map crafted there would use (a grid of 128 << scale blocks)
Int32_2 SnapCenter(const Int32_2 _origin, const uint8_t _scale);
// The world coordinate (on one axis) of the top-left of a pixel row/column. _pixel may be -1 (the row before the first)
int PixelToWorld(const int _pixel, const int _centerAxis, const uint8_t _scale);
// Marker coordinates for a world position, or nothing if it's off the map
std::optional<Byte2> WorldToMarker(const double _wx, const double _wz, const Int32_2 _center, const uint8_t _scale);

MarkerPlacement MakeMarker(const Icon _icon, const float _rotation, const Byte2 _position);
void AppendPixel(std::vector<uint8_t>& _mapData, const uint8_t _value);
void InitGraphics(std::vector<uint8_t>& _mapData, const Byte2 _offset);
void PlaceMarkers(std::vector<uint8_t>& _mapData, const std::vector<MarkerPlacement>& _markers);

void InitMaps();
// Allocates a new map centred on _origin (at the default scale) and returns its id. Ids start at 1, 0 means "no map yet"
int16_t CreateMap(const Int32_2 _origin);
// Returns the map with this id, or nullptr if it doesn't exist
struct Item* GetMap(const int16_t _id);
// Used for held map stacks. Returns _id if that map exists. Otherwise (a freshly crafted map has id 0, and maps
// are not saved yet so ids can be unknown after a restart) the map is created with _origin as its origin
int16_t ResolveMap(const int16_t _id, const Int32_2 _origin);
struct Item {
	static constexpr uint8_t MAP_ITEM_WIDTH = 128;
	static constexpr uint16_t MAP_ITEM_AREA = MAP_ITEM_WIDTH * MAP_ITEM_WIDTH;
	// B1.7.3 maps are 1:8 (each pixel covers 8x8 blocks)
	static constexpr uint8_t DEFAULT_SCALE = 3;
	// Map image data
	// TODO: Could THEORETICALLY be backed into a 5-bit array (max of 27 materials)
	std::array<MaterialType, MAP_ITEM_AREA> data{};
	// Map origin (where it was first created). The map is centred on this snapped to a grid, see GetCenter()
	Int32_2 origin = {0,0};
	// Map scale (0-4, aka 1,2,4,8,16). Needs 3 bits, and must be unsigned
	uint8_t scale : 3 = 0;
	// If anything on the map has changed and updates need to be sent out
	bool dirty : 1 = true;
	void Init(const Int32_2 _origin, const uint8_t _scale = DEFAULT_SCALE);
	// The world position at the centre of the map
	Int32_2 GetCenter() const;
	void Tick();
};

extern std::map<int16_t, Map::Item> mapItems;
}; // namespace Map