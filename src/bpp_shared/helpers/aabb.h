/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once
#include <vector>
#include "direction.h"
#include "numeric_structs.h"
#include <optional>

struct AABBIntercept {
	Vec3 point;
	Cardinal::Value face = Cardinal::Value::None;
};

struct AABB {
	double minX, minY, minZ;
	double maxX, maxY, maxZ;

	std::optional<AABBIntercept> CalculateIntercept(const Vec3& _from, const Vec3& _to) const {
		const double mins[3] = { minX, minY, minZ };
		const double maxs[3] = { maxX, maxY, maxZ };
		// Vanilla side ids 4/5 (x), 0/1 (y), 2/3 (z)
		const Cardinal::Value minFaces[3] = { Cardinal::Value::West, Cardinal::Value::Down, Cardinal::Value::North };
		const Cardinal::Value maxFaces[3] = { Cardinal::Value::East, Cardinal::Value::Up, Cardinal::Value::South };

		std::optional<AABBIntercept> best;
		double bestDist = 0.0;
		for (int axis = 0; axis < 3; axis++) {
			for (int side = 0; side < 2; side++) {
				auto point = _from.GetIntermediateOnAxis(_to, axis, side == 0 ? mins[axis] : maxs[axis]);
				if (!point)
					continue;

				// Must land on the face
				bool onFace = true;
				for (int other = 0; other < 3; other++) {
					if (other != axis && ((*point)[other] < mins[other] || (*point)[other] > maxs[other]))
						onFace = false;
				}
				if (!onFace)
					continue;

				double dist = _from.DistanceSquared(*point);
				if (!best || dist < bestDist) {
					best = AABBIntercept{ *point, side == 0 ? minFaces[axis] : maxFaces[axis] };
					bestDist = dist;
				}
			}
		}
		return best;
	}

	bool Intersects(const AABB& _other) const {
		return (_other.maxX > minX && _other.minX < maxX && _other.maxY > minY && _other.minY < maxY &&
		        _other.maxZ > minZ && _other.minZ < maxZ);
	}

	bool PointIntersects(const Vec3& _vec) const {
		return (_vec.x > minX && _vec.x < maxX) && (_vec.y > minY && _vec.y < maxY) && (_vec.z > minZ && _vec.z < maxZ);
	}

	AABB Offset(double _dx, double _dy, double _dz) const {
		return { minX + _dx, minY + _dy, minZ + _dz, maxX + _dx, maxY + _dy, maxZ + _dz };
	}

	AABB Expand(double _dx, double _dy, double _dz) const {
		return { minX - _dx, minY - _dy, minZ - _dz, maxX + _dx, maxY + _dy, maxZ + _dz };
	}

	AABB AddCoord(double _dx, double _dy, double _dz) const {
		double x0 = minX, y0 = minY, z0 = minZ;
		double x1 = maxX, y1 = maxY, z1 = maxZ;
		if (_dx < 0)
			x0 += _dx;
		else
			x1 += _dx;
		if (_dy < 0)
			y0 += _dy;
		else
			y1 += _dy;
		if (_dz < 0)
			z0 += _dz;
		else
			z1 += _dz;
		return { x0, y0, z0, x1, y1, z1 };
	}

	// Checks YZ overlap first, then computes how far we can move in X
	double CalculateXOffset(const AABB& _other, double _dx) const {
		if (_other.maxY > minY && _other.minY < maxY) {
			if (_other.maxZ > minZ && _other.minZ < maxZ) {
				if (_dx > 0.0 && _other.maxX <= minX) {
					double d = minX - _other.maxX;
					if (d < _dx)
						_dx = d;
				}
				if (_dx < 0.0 && _other.minX >= maxX) {
					double d = maxX - _other.minX;
					if (d > _dx)
						_dx = d;
				}
			}
		}
		return _dx;
	}

	double CalculateYOffset(const AABB& _other, double _dy) const {
		if (_other.maxX > minX && _other.minX < maxX) {
			if (_other.maxZ > minZ && _other.minZ < maxZ) {
				if (_dy > 0.0 && _other.maxY <= minY) {
					double d = minY - _other.maxY;
					if (d < _dy)
						_dy = d;
				}
				if (_dy < 0.0 && _other.minY >= maxY) {
					double d = maxY - _other.minY;
					if (d > _dy)
						_dy = d;
				}
			}
		}
		return _dy;
	}

	double CalculateZOffset(const AABB& _other, double _dz) const {
		if (_other.maxX > minX && _other.minX < maxX) {
			if (_other.maxY > minY && _other.minY < maxY) {
				if (_dz > 0.0 && _other.maxZ <= minZ) {
					double d = minZ - _other.maxZ;
					if (d < _dz)
						_dz = d;
				}
				if (_dz < 0.0 && _other.minZ >= maxZ) {
					double d = maxZ - _other.minZ;
					if (d > _dz)
						_dz = d;
				}
			}
		}
		return _dz;
	}
};

// A collision shape is basically a container of AABB's, used for blocks like stairs and to generate the swept volume of an entity's movement.
struct CollisionShape {
	std::vector<AABB> boxes;

	void Add(const AABB& _box) {
		boxes.push_back(_box);
	}

	CollisionShape Offset(double _dx, double _dy, double _dz) const {
		CollisionShape result;
		result.boxes.reserve(boxes.size());
		for (const auto& box : boxes)
			result.boxes.push_back(box.Offset(_dx, _dy, _dz));
		return result;
	}

	bool Intersects(const CollisionShape& _other) const {
		for (const auto& box1 : boxes)
			for (const auto& box2 : _other.boxes)
				if (box1.Intersects(box2))
					return true;
		return false;
	}

	bool Intersects(const AABB& _other) const {
		for (const auto& box1 : boxes) {
			if (box1.Intersects(_other)) {
				return true;
			}
		}
		return false;
	}

	bool PointIntersects(const Vec3& _vec) const {
		for (const auto& box1 : boxes) {
			if (box1.PointIntersects(_vec)) {
				return true;
			}
		}
		return false;
	}

	std::optional<AABB> GetBounds() const {
		if (boxes.empty())
			return std::nullopt;
		AABB bounds = boxes[0];
		for (const auto& box : boxes) {
			bounds.minX = std::min(bounds.minX, box.minX);
			bounds.minY = std::min(bounds.minY, box.minY);
			bounds.minZ = std::min(bounds.minZ, box.minZ);
			bounds.maxX = std::max(bounds.maxX, box.maxX);
			bounds.maxY = std::max(bounds.maxY, box.maxY);
			bounds.maxZ = std::max(bounds.maxZ, box.maxZ);
		}
		return bounds;
	}

	CollisionShape Expand(double _dx, double _dy, double _dz) const {
		CollisionShape result;
		result.boxes.reserve(boxes.size());
		for (const auto& box : boxes)
			result.boxes.push_back(box.Expand(_dx, _dy, _dz));
		return result;
	}

	double CalculateXOffset(const AABB& _entity, double _dx) const {
		for (const auto& box : boxes)
			_dx = box.CalculateXOffset(_entity, _dx);
		return _dx;
	}

	double CalculateYOffset(const AABB& _entity, double _dy) const {
		for (const auto& box : boxes)
			_dy = box.CalculateYOffset(_entity, _dy);
		return _dy;
	}

	double CalculateZOffset(const AABB& _entity, double _dz) const {
		for (const auto& box : boxes)
			_dz = box.CalculateZOffset(_entity, _dz);
		return _dz;
	}

	bool IsEmpty() const {
		return boxes.empty();
	}
};