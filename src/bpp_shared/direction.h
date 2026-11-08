/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 * 
*/

#pragma once
#include "./helpers/java/java_math.h"
#include <array>
#include <string_view>
namespace Axis {
// Axis Directions
enum class Value : uint8_t {
	NorthSouth = 0,
	EastWest = 1,
	UpDown = 2,
	None,
};
} // namespace Axis

namespace Cardinal {
// Cardinal Directions
enum class Value : uint8_t {
	// Do NOT change this order!
	// This order allows us to get the
	// opposite direction with a single XOR!
	North = 0,
	South = 1,
	East = 2,
	West = 3,
	Up = 4,
	Down = 5,
	None,
};
inline constexpr Value ALL[6] = { Value::North, Value::South, Value::East, Value::West, Value::Up, Value::Down };
inline constexpr Value HORIZONTAL[4] = { Value::North, Value::South, Value::East, Value::West };

Value Opposite(const Value _dir) noexcept;
Value ToLeft(const Value _dir) noexcept;
Value ToRight(const Value _dir) noexcept;
Value FromAngle(const float _yaw) noexcept;
bool IsHorizontal(const Value _dir) noexcept;
bool IsVertical(const Value _dir) noexcept;
Axis::Value ToAxis(const Value _dir) noexcept;
std::string_view Str(const Value _dir) noexcept;
} // namespace Cardinal

namespace Axis {
// Axis Directions
inline constexpr Value ALL[3] = { Value::NorthSouth, Value::EastWest, Value::UpDown };
inline constexpr Value HORIZONTAL[2] = { Value::NorthSouth, Value::EastWest };

Value ToLeft(const Value _axis) noexcept;
Value ToRight(const Value _axis) noexcept;
Value FromAngle(const float _yaw) noexcept;
bool IsHorizontal(const Value _axis) noexcept;
bool IsVertical(const Value _axis) noexcept;
std::array<Cardinal::Value, 2> ToCardinal(const Value _axis) noexcept;
std::string_view Str(const Value _axis) noexcept;
} // namespace Axis