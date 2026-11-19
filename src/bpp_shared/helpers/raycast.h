/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#pragma once
#include "../direction.h"
#include "aabb.h"
#include "base_structs.h"
#include "blocks.h"
#include "blocks/block_behaviors.h"
#include "blocks/block_properties.h"
#include "blocks/materials.h"
#include "helpers/java/java_math.h"
#include "numeric_structs.h"
#include "world.h"
#include <cmath>
#include <cstdint>
#include <optional>

enum class RayCastMode : uint8_t {
	IGNORE_FLUIDS,
	ACCEPT_SOURCES,
	ACCEPT_ANY
};

struct RayCastResult {
	bool hit = false;
	Block hitBlock = {};
	Int3 blockPosition = {};
	Cardinal::Value face = Cardinal::Value::None;
	Vec3 hitPosition = {};
};

// Name is a bit obvious isn't it?
namespace Raycast {

// Whether the fluid block should be treated as solid for this raycast mode.
inline bool ShouldConsiderFluid(RayCastMode _mode, uint8_t _meta) {
	switch (_mode) {
	case RayCastMode::IGNORE_FLUIDS:
		return false;
	case RayCastMode::ACCEPT_SOURCES:
		return _meta == 0;
	case RayCastMode::ACCEPT_ANY:
	default:
		return true;
	}
}

inline RayCastResult Raycast(WorldManager& _world, Vec3 _startPos, Vec3 _endPos, RayCastMode _mode,
                             bool _ignoreNonCollidable = false) {
	RayCastResult result;
	if (std::isnan(_startPos.x) || std::isnan(_startPos.y) || std::isnan(_startPos.z) || std::isnan(_endPos.x) ||
	    std::isnan(_endPos.y) || std::isnan(_endPos.z))
		return result;

	auto traceBlock = [&](Int3 _pos, const Vec3& _from) -> bool {
		BlockType id = _world.GetBlockId(_pos);
		if (id == BlockType::BLOCK_AIR)
			return false;
		uint8_t meta = _world.GetMetadata(_pos);

		if (_ignoreNonCollidable && Blocks::blockBehaviors[id].getCollider(meta).IsEmpty())
			return false;

		// Fire is never hit
		if (id == BLOCK_FIRE)
			return false;
		if (Blocks::blockProperties[id].material.isLiquid && !ShouldConsiderFluid(_mode, meta))
			return false;

		Vec3 offset = { double(_pos.x), double(_pos.y), double(_pos.z) };
		AABB bounds = Blocks::blockBehaviors[id].getRayBounds(meta);
		auto hit = bounds.CalculateIntercept(_from - offset, _endPos - offset);
		if (!hit)
			return false;

		result.hit = true;
		result.hitBlock = { id, meta };
		result.blockPosition = _pos;
		result.face = hit->face;
		result.hitPosition = hit->point + offset;
		return true;
	};

	const int endX = MathHelper::FloorDouble(_endPos.x);
	const int endY = MathHelper::FloorDouble(_endPos.y);
	const int endZ = MathHelper::FloorDouble(_endPos.z);
	int x = MathHelper::FloorDouble(_startPos.x);
	int y = MathHelper::FloorDouble(_startPos.y);
	int z = MathHelper::FloorDouble(_startPos.z);

	// The block we start in is tested first, so a ray starting inside a block still hits its exit face
	if (traceBlock({ x, y, z }, _startPos))
		return result;

	for (int steps = 200; steps-- >= 0;) {
		if (std::isnan(_startPos.x) || std::isnan(_startPos.y) || std::isnan(_startPos.z))
			return result;
		if (x == endX && y == endY && z == endZ)
			return result;

		// The next boundary on each axis we still have to cross
		bool crossX = true, crossY = true, crossZ = true;
		double boundX = 999.0, boundY = 999.0, boundZ = 999.0;
		if (endX > x)
			boundX = double(x) + 1.0;
		else if (endX < x)
			boundX = double(x) + 0.0;
		else
			crossX = false;
		if (endY > y)
			boundY = double(y) + 1.0;
		else if (endY < y)
			boundY = double(y) + 0.0;
		else
			crossY = false;
		if (endZ > z)
			boundZ = double(z) + 1.0;
		else if (endZ < z)
			boundZ = double(z) + 0.0;
		else
			crossZ = false;

		// How far along the remaining segment each boundary is
		double tX = 999.0, tY = 999.0, tZ = 999.0;
		double dx = _endPos.x - _startPos.x;
		double dy = _endPos.y - _startPos.y;
		double dz = _endPos.z - _startPos.z;
		if (crossX)
			tX = (boundX - _startPos.x) / dx;
		if (crossY)
			tY = (boundY - _startPos.y) / dy;
		if (crossZ)
			tZ = (boundZ - _startPos.z) / dz;

		// Move the start point onto the nearest boundary. Vanilla side ids: 4/5 = x, 0/1 = y, 2/3 = z
		int side;
		if (tX < tY && tX < tZ) {
			side = endX > x ? 4 : 5;
			_startPos.x = boundX;
			_startPos.y += dy * tX;
			_startPos.z += dz * tX;
		} else if (tY < tZ) {
			side = endY > y ? 0 : 1;
			_startPos.x += dx * tY;
			_startPos.y = boundY;
			_startPos.z += dz * tY;
		} else {
			side = endZ > z ? 2 : 3;
			_startPos.x += dx * tZ;
			_startPos.y += dy * tZ;
			_startPos.z = boundZ;
		}

		// Work out which cell we just entered
		x = MathHelper::FloorDouble(_startPos.x);
		if (side == 5)
			x--;
		y = MathHelper::FloorDouble(_startPos.y);
		if (side == 1)
			y--;
		z = MathHelper::FloorDouble(_startPos.z);
		if (side == 3)
			z--;

		if (traceBlock({ x, y, z }, _startPos))
			return result;
	}

	return result;
}

} // namespace Raycast