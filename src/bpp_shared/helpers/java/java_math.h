/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

// Library for emulating Java/Java Edition math functions

/**
 * @brief Linear interpolation function
 * 
 * @param t Interpolation factor
 * @param a Start value (t = 0.0)
 * @param b End value (t = 1.0)
 * @return Interpolated value between a and b
 */
template <typename T>
constexpr inline T Lerp(const T _t, const T _a, const T _b) {
	return _a + _t * (_b - _a);
}

/**
 * @brief Java-equivalent functions
 * 
 */
namespace Java {
// The following should be somewhat faithful implementation of
// Java's casting functions, as defined in
// "Chapter 5. Conversions and Contexts"
/**
	 * @brief Casts a double to a 64-bit integer
	 */
constexpr inline int64_t DoubleToInt64(const double _value) noexcept {
	constexpr double LIMIT = 0x1p63; // Exactly 2^63
	if (_value >= LIMIT) [[unlikely]]
		return INT64_MAX;
	if (_value <= -LIMIT) [[unlikely]]
		return INT64_MIN;
	if (_value != _value) [[unlikely]]
		return 0;
	return static_cast<int64_t>(_value);
}
/**
	 * @brief Casts a double to a 32-bit integer
	 */
constexpr inline int32_t DoubleToInt32(const double _value) {
	constexpr double LIMIT = 0x1p31;
	if (_value >= LIMIT) [[unlikely]]
		return INT32_MAX;
	if (_value <= -LIMIT) [[unlikely]]
		return INT32_MIN;
	if (_value != _value) [[unlikely]]
		return 0;
	return static_cast<int32_t>(_value);
}
/**
	 * @brief Casts a float to a 64-bit integer
	 */
constexpr inline int64_t FloatToInt64(const float _value) {
	constexpr float LIMIT = 0x1p63;
	if (_value >= LIMIT) [[unlikely]]
		return INT64_MAX;
	if (_value <= -LIMIT) [[unlikely]]
		return INT64_MIN;
	if (_value != _value) [[unlikely]]
		return 0;
	return static_cast<int64_t>(_value);
}
/**
	 * @brief Casts a float to a 32-bit integer
	 */
constexpr inline int32_t FloatToInt32(const float _value) {
	constexpr float LIMIT = 0x1p31;
	if (_value >= LIMIT) [[unlikely]]
		return INT32_MAX;
	if (_value <= -LIMIT) [[unlikely]]
		return INT32_MIN;
	if (_value != _value) [[unlikely]]
		return 0;
	return static_cast<int32_t>(_value);
}
}; // namespace Java

/**
* @brief Java-equivalent hashing function
* 
* @param value The input string
* @return Hashed string expressed as an integer
*/
inline int32_t HashCode(const std::string _value) {
	int32_t h = 0;
	if (h == 0 && _value.size() > 0) {
		for (size_t i = 0; i < _value.size(); i++) {
			h = 31 * h + _value[i];
		}
	}
	return h;
}

/**
 * @brief A struct that's used like Javas Math.java library
 * 
 */
struct JavaMath {
	static constexpr double PI = 3.141592653589793;
	static constexpr float PI_FLOAT = float(PI);
	static constexpr inline int32_t Abs(int32_t _a) {
		return (_a < 0) ? -_a : _a;
	}
};

/**
 * @brief A small helper that's used to simplify or speed up some code
 * 
 */
struct MathHelper {
	static constexpr size_t TABLE_SIZE = 65536;
	// Requires C++17
	inline static std::array<float, TABLE_SIZE> m_SIN_TABLE{};

	static constexpr inline float Sin(float _x) {
		return m_SIN_TABLE[Java::FloatToInt32(_x * 10430.378f) & 0xFFFF];
	}

	static constexpr inline float Cos(float _x) {
		return m_SIN_TABLE[(Java::FloatToInt32(_x * 10430.378f + 16384.0f)) & 0xFFFF];
	}

	static inline float SqrtFloat(float _x) {
		return std::sqrt(_x);
	}

	static inline float SqrtDouble(double _x) {
		return static_cast<float>(std::sqrt(_x));
	}

	static constexpr inline int32_t FloorFloat(float _x) {
		int32_t i = Java::FloatToInt32(_x);
		return _x < static_cast<float>(i) ? i - 1 : i;
	}

	static constexpr inline int32_t FloorDouble(double _x) {
		int32_t i = Java::DoubleToInt32(_x);
		return _x < static_cast<double>(i) ? i - 1 : i;
	}

	static constexpr inline float Abs(float _x) {
		return _x >= 0.0f ? _x : -_x;
	}

	static constexpr inline double AbsMax(double _a, double _b) {
		if (_a < 0.0)
			_a = -_a;
		if (_b < 0.0)
			_b = -_b;
		return _a > _b ? _a : _b;
	}

	static inline void InitSinTable() {
		for (size_t i = 0; i < MathHelper::TABLE_SIZE; ++i)
			MathHelper::m_SIN_TABLE[i] = float(
			    std::sin(double(i) * JavaMath::PI * 2.0 / double(MathHelper::TABLE_SIZE)));
	}
};
