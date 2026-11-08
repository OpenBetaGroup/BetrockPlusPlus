/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 * 
*/

#include "direction.h"

// Cardinal Directions
namespace Cardinal {
Value Opposite(const Value _dir) noexcept {
	if (_dir == Value::None)
		return Value::None;
	return static_cast<Value>(static_cast<uint8_t>(_dir) ^ 1);
}

// Rotating around the vertical (Y) axis, as seen from above.
Value ToLeft(const Value _dir) noexcept {
	switch (_dir) {
	case Value::North:
		return Value::West;
	case Value::South:
		return Value::East;
	case Value::East:
		return Value::North;
	case Value::West:
		return Value::South;
	default:
		return _dir;
	}
}

Value ToRight(const Value _dir) noexcept {
	switch (_dir) {
	case Value::North:
		return Value::East;
	case Value::South:
		return Value::West;
	case Value::East:
		return Value::South;
	case Value::West:
		return Value::North;
	default:
		return _dir;
	}
}

Value FromAngle(const float _yaw) noexcept {
	switch (MathHelper::FloorDouble((_yaw + 180.0f) * 4.0f / 360.0f - 0.5f) & 0b11) {
	case 0:
		return Value::East;
	case 1:
		return Value::South;
	case 2:
		return Value::West;
	case 3:
		return Value::North;
	default:
		return Value::None;
	}
}

bool IsHorizontal(const Value _dir) noexcept {
	switch (_dir) {
	case Value::North:
	case Value::South:
	case Value::East:
	case Value::West:
		return true;
	default:
		return false;
	}
}

bool IsVertical(const Value _dir) noexcept {
	switch (_dir) {
	case Value::Up:
	case Value::Down:
		return true;
	default:
		return false;
	}
}

Axis::Value ToAxis(const Value _dir) noexcept {
	switch (_dir) {
	case Value::North:
		return Axis::Value::NorthSouth;
	case Value::South:
		return Axis::Value::NorthSouth;
	case Value::East:
		return Axis::Value::EastWest;
	case Value::West:
		return Axis::Value::EastWest;
	case Value::Up:
		return Axis::Value::UpDown;
	case Value::Down:
		return Axis::Value::UpDown;
	default:
		return Axis::Value::None;
	}
}

std::string_view Str(const Value _dir) noexcept {
	switch (_dir) {
	case Value::North:
		return "North";
	case Value::South:
		return "South";
	case Value::East:
		return "East";
	case Value::West:
		return "West";
	case Value::Up:
		return "Up";
	case Value::Down:
		return "Down";
	default:
		return "INVALID";
	}
}

} // namespace Cardinal

// Axis Directions
namespace Axis {
Value ToLeft(const Value _axis) noexcept {
	switch (_axis) {
	case Value::NorthSouth:
		return Value::EastWest;
	case Value::EastWest:
		return Value::NorthSouth;
	default:
		return _axis;
	}
}

Value ToRight(const Value _axis) noexcept {
	return ToLeft(_axis);
}

Value FromAngle(const float _yaw) noexcept {
	switch (Cardinal::FromAngle(_yaw)) {
	case Cardinal::Value::East:
		return Value::EastWest;
	case Cardinal::Value::South:
		return Value::NorthSouth;
	case Cardinal::Value::West:
		return Value::EastWest;
	case Cardinal::Value::North:
		return Value::NorthSouth;
	default:
		return Value::None;
	}
}

bool IsHorizontal(const Value _axis) noexcept {
	switch (_axis) {
	case Value::NorthSouth:
	case Value::EastWest:
		return true;
	default:
		return false;
	}
}

bool IsVertical(const Value _axis) noexcept {
	return _axis == Value::UpDown;
}

std::array<Cardinal::Value, 2> ToCardinal(const Value _dir) noexcept {
	switch (_dir) {
	case Value::NorthSouth:
		return { Cardinal::Value::North, Cardinal::Value::South };
	case Value::EastWest:
		return { Cardinal::Value::East, Cardinal::Value::West };
	case Value::UpDown:
		return { Cardinal::Value::Up, Cardinal::Value::Down };
	default:
		return { Cardinal::Value::None, Cardinal::Value::None };
	}
}

std::string_view Str(const Value _axis) noexcept {
	switch (_axis) {
	case Value::NorthSouth:
		return "North/South";
	case Value::EastWest:
		return "East/West";
	case Value::UpDown:
		return "Up/Down";
	default:
		return "INVALID";
	}
}

} // namespace Axis