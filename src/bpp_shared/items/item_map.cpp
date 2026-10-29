/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "item_map.h"
#include "../enums/network/packet_data.h"
#include "../logger/logger.h"
#include <algorithm>
#include <cmath>

namespace Map {

Int32_2 SnapCenter(const Int32_2 _origin, const uint8_t _scale) {
	const int span = Item::MAP_ITEM_WIDTH << _scale;
	auto snap = [span](const int _v) { return static_cast<int>(std::lround(static_cast<double>(_v) / span)) * span; };
	return Int32_2{ snap(_origin.x), snap(_origin.z) };
}

int PixelToWorld(const int _pixel, const int _centerAxis, const uint8_t _scale) {
	const int blocksPerPixel = 1 << _scale;
	// The centre is always a multiple of the map span, so this divides exactly
	return ((_centerAxis / blocksPerPixel) - (Item::MAP_ITEM_WIDTH / 2) + _pixel) * blocksPerPixel;
}

std::optional<Byte2> WorldToMarker(const double _wx, const double _wz, const Int32_2 _center, const uint8_t _scale) {
	const double blocksPerPixel = static_cast<double>(1 << _scale);
	const double dx = (_wx - _center.x) / blocksPerPixel;
	const double dz = (_wz - _center.z) / blocksPerPixel;
	constexpr double HALF = Item::MAP_ITEM_WIDTH / 2.0;
	if (dx < -HALF || dx > HALF || dz < -HALF || dz > HALF)
		return std::nullopt;
	// Half-pixel units, so the full -64..64 range maps onto -128..127
	return Byte2{ static_cast<int8_t>(std::clamp(std::lround(dx * 2.0), -128L, 127L)),
		          static_cast<int8_t>(std::clamp(std::lround(dz * 2.0), -128L, 127L)) };
}

MarkerPlacement MakeMarker(const Icon _icon, const float _rotation, const Byte2 _position) {
	// fmod keeps the sign of the input, so negative yaw must be wrapped into [0, 360) first.
	// (Casting a negative float to uint8_t is undefined behaviour)
	float wrapped = std::fmod(_rotation, 360.0f);
	if (wrapped < 0.0f)
		wrapped += 360.0f;
	// 16 steps, and 16 wraps back round to 0
	uint8_t rot = static_cast<uint8_t>(std::lround((wrapped / 360.0f) * 16.0f)) & 0x0F;
	return MarkerPlacement{ .icon = _icon, .rotation = rot, .position = _position };
}

void AppendPixel(std::vector<uint8_t>& _mapData, const uint8_t _value) {
	if (_mapData.size() >= MAX_PACKET_DATA) {
		Logger::Warning << "Attempted to write too much map data in a single packet! Rejected! (" << _mapData.size()
		                << "/" << MAX_PACKET_DATA << ")\n";
		return;
	}
	_mapData.push_back(_value);
}

void InitGraphics(std::vector<uint8_t>& _mapData, const Byte2 _offset) {
	_mapData = { PacketData::MapDataType::GRAPHICS, static_cast<uint8_t>(_offset.x), static_cast<uint8_t>(_offset.z) };
}

void PlaceMarkers(std::vector<uint8_t>& _mapData, const std::vector<MarkerPlacement>& _markers) {
	// 1 type byte + 3 bytes per marker, and it all has to fit in one packet
	const size_t count = std::min(_markers.size(), (MAX_PACKET_DATA - 1) / 3);
	_mapData.clear();
	_mapData.reserve(1 + (count * 3));
	_mapData.push_back(PacketData::MapDataType::ICON);
	for (size_t i = 0; i < count; i++) {
		const auto& m = _markers[i];
		_mapData.push_back(static_cast<uint8_t>(((m.rotation & 0x0F) << 4) | (static_cast<uint8_t>(m.icon) & 0x0F)));
		_mapData.push_back(m.position.x);
		_mapData.push_back(m.position.z);
	}
}

// Held item maps
std::map<int16_t, Item> mapItems = {};
static int16_t globalId = 0;

void InitMaps() {
	// TODO: Find the highest map value from files (this can wrap around)
	globalId = 0;
}
int16_t CreateMap(const Int32_2 _origin) {
	// Ids wrap, so skip any that are still in use
	do {
		globalId = (globalId == INT16_MAX) ? 1 : globalId + 1;
	} while (mapItems.contains(globalId));
	// Construct in place, an Item is 16KB so don't build one on the stack and copy it
	mapItems[globalId].Init(_origin);
	return globalId;
}

Item* GetMap(const int16_t _id) {
	auto it = mapItems.find(_id);
	return it == mapItems.end() ? nullptr : &it->second;
}

int16_t ResolveMap(const int16_t _id, const Int32_2 _origin) {
	if (_id > 0) {
		if (mapItems.contains(_id))
			return _id;
		// Unknown id, most likely from before a restart. Keep the id the stack already has
		mapItems[_id].Init(_origin);
		return _id;
	}
	return CreateMap(_origin);
}

void Item::Init(const Int32_2 _origin, const uint8_t _scale) {
	// TODO: Check file for existing map
	// Nope, this is a new map, so lets go
	origin = _origin;
	scale = _scale;
	data.fill(MaterialType::Air);
	dirty = true;
}

Int32_2 Item::GetCenter() const {
	return SnapCenter(origin, scale);
}

void Item::Tick() {
	if (!dirty)
		return;
}
} // namespace Map
