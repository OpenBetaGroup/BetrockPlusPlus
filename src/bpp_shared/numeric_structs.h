/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 * 
*/

#pragma once
#include "direction.h"
#include <cmath>
#include <cstdint>
#include <optional>
#include <ostream>
#include <sstream>
#include <vector>

#if defined(__has_include)
#if __has_include(<glm/glm.hpp>)
#include <glm/glm.hpp>
#define TRINUM_HAS_GLM 1
#endif
#endif

/**
 * @brief A struct that contains three numbers (x,y,z)
 * 
 */
template <typename T = int>
struct TriNumber {
	union {
		// For accessing directly
		struct {
			T x, y, z;
		};
		// For accessing as array
		T data[3];
	};

	constexpr TriNumber(T _x, T _y, T _z) : x(_x), y(_y), z(_z) {}
	constexpr TriNumber() : x(0), y(0), z(0) {}

	bool operator==(const TriNumber& _other) const {
		return x == _other.x && y == _other.y && z == _other.z;
	}

	TriNumber operator+(const TriNumber& _other) const {
		return TriNumber{ x + _other.x, y + _other.y, z + _other.z };
	}

	TriNumber operator-(const TriNumber& _other) const {
		return TriNumber{ x - _other.x, y - _other.y, z - _other.z };
	}

	TriNumber operator*(const TriNumber& _other) const {
		return TriNumber{ x * _other.x, y * _other.y, z * _other.z };
	}

	TriNumber operator/(const TriNumber& _other) const {
		return TriNumber{ x / _other.x, y / _other.y, z / _other.z };
	}

	TriNumber operator+=(const TriNumber& _other) {
		x += _other.x;
		y += _other.y;
		z += _other.z;
		return *this;
	}

	TriNumber operator-=(const TriNumber& _other) {
		x -= _other.x;
		y -= _other.y;
		z -= _other.z;
		return *this;
	}

	TriNumber operator*=(const TriNumber& _other) {
		x *= _other.x;
		y *= _other.y;
		z *= _other.z;
		return *this;
	}

	TriNumber operator/=(const TriNumber& _other) {
		x /= _other.x;
		y /= _other.y;
		z /= _other.z;
		return *this;
	}

	// Allows for tri + 1 = (x+1,y+1,z+1)
	template <typename U>
	auto operator-(const U& _other) const {
		using R = std::common_type_t<T, U>;
		return TriNumber<R>{ static_cast<R>(x) - _other, static_cast<R>(y) - _other, static_cast<R>(z) - _other };
	}

	// Allows for tri - 1 = (x-1,y-1,z-1)
	template <typename U>
	auto operator+(const U& _other) const {
		using R = std::common_type_t<T, U>;
		return TriNumber<R>{ static_cast<R>(x) + _other, static_cast<R>(y) + _other, static_cast<R>(z) + _other };
	}

	// Allows for tri * 2 = (x*2,y*2,z*2)
	template <typename U>
	auto operator*(const U& _other) const {
		using R = std::common_type_t<T, U>;
		return TriNumber<R>{ static_cast<R>(x) * _other, static_cast<R>(y) * _other, static_cast<R>(z) * _other };
	}

	// Allows for tri / 2 = (x/2,y/2,z/2)
	template <typename U>
	auto operator/(const U& _other) const {
		using R = std::common_type_t<T, U>;
		return TriNumber<R>{ static_cast<R>(x) / _other, static_cast<R>(y) / _other, static_cast<R>(z) / _other };
	}

	template <typename U>
	auto operator-=(const U& _other) {
		using R = std::common_type_t<T, U>;
		x -= static_cast<R>(_other);
		y -= static_cast<R>(_other);
		z -= static_cast<R>(_other);
		return *this;
	}

	template <typename U>
	auto operator+=(const U& _other) {
		using R = std::common_type_t<T, U>;
		x += static_cast<R>(_other);
		y += static_cast<R>(_other);
		z += static_cast<R>(_other);
		return *this;
	}

	template <typename U>
	auto operator*=(const U& _other) {
		using R = std::common_type_t<T, U>;
		x *= static_cast<R>(_other);
		y *= static_cast<R>(_other);
		z *= static_cast<R>(_other);
		return *this;
	}

	template <typename U>
	auto operator/=(const U& _other) {
		using R = std::common_type_t<T, U>;
		x /= static_cast<R>(_other);
		y /= static_cast<R>(_other);
		z /= static_cast<R>(_other);
		return *this;
	}

#ifdef TRINUM_HAS_GLM
	// Construct from any glm::vec3-shaped type (vec3, dvec3, ivec3, ...)
	template <typename U, glm::qualifier Q>
	constexpr TriNumber(const glm::vec<3, U, Q>& _v)
	    : x(static_cast<T>(_v.x)), y(static_cast<T>(_v.y)), z(static_cast<T>(_v.z)) {}

	// Convert to any glm::vec3-shaped type (deduced from assignment target)
	template <typename U, glm::qualifier Q>
	operator glm::vec<3, U, Q>() const {
		return glm::vec<3, U, Q>(static_cast<U>(x), static_cast<U>(y), static_cast<U>(z));
	}
#endif

	friend std::ostream& operator<<(std::ostream& _os, const TriNumber& _val) {
		if constexpr (std::is_integral_v<T>) {
			_os << "(" << static_cast<int64_t>(_val.x) << ", " << static_cast<int64_t>(_val.y) << ", "
			    << static_cast<int64_t>(_val.z) << ")";
		} else {
			_os << "(" << _val.x << ", " << _val.y << ", " << _val.z << ")";
		}
		return _os;
	}

	T& operator[](int _axis) {
		return data[_axis];
	}
	const T& operator[](int _axis) const {
		return data[_axis];
	}

	std::string Str() const {
		std::ostringstream oss;
		oss << *this; // Use the overloaded << operator
		return oss.str();
	}

	T Total() const {
		return x * y * z;
	}

	double DistanceSquared(const TriNumber& _other) const {
		double dx = double(x) - double(_other.x);
		double dy = double(y) - double(_other.y);
		double dz = double(z) - double(_other.z);
		return dx * dx + dy * dy + dz * dz;
	}

	double Distance(const TriNumber& _other) const {
		return std::sqrt(DistanceSquared(_other));
	}

	std::optional<TriNumber> GetIntermediateOnAxis(const TriNumber& _to, int _axis, double _value) const {
		double dx = double(_to.x) - double(x);
		double dy = double(_to.y) - double(y);
		double dz = double(_to.z) - double(z);
		double delta = _axis == 0 ? dx : (_axis == 1 ? dy : dz);
		if (delta * delta < 1.0000000116860974E-7)
			return std::nullopt;
		double t = (_value - double(data[_axis])) / delta;
		if (t < 0.0 || t > 1.0)
			return std::nullopt;
		return TriNumber(T(double(x) + dx * t), T(double(y) + dy * t), T(double(z) + dz * t));
	}

	double Length() const {
		return std::sqrt(x * x + y * y + z * z);
	}

	auto Offset(const Cardinal::Value _direction, const T _value = 1) {
		switch (_direction) {
		case Cardinal::Value::None:
			break;
		case Cardinal::Value::North:
			z -= _value;
			break;
		case Cardinal::Value::South:
			z += _value;
			break;
		case Cardinal::Value::East:
			x += _value;
			break;
		case Cardinal::Value::West:
			x -= _value;
			break;
		case Cardinal::Value::Up:
			y += _value;
			break;
		case Cardinal::Value::Down:
			y -= _value;
			break;
		}
		return *this;
	}

	auto WithOffset(const Cardinal::Value _direction, const T _value = 1) const {
		TriNumber<T> result = *this;
		return result.Offset(_direction, _value);
	}

	// Returns the directions that the vector is pointing towards
	std::array<Cardinal::Value, 3> GetFacingDirections() {
		// Within a reasonable, 3-dimensional world,
		// a vector should only ever point in, at most, three directions
		std::array<Cardinal::Value, 3> dirs;
		int8_t dirIndex = 0;
		T length = Length();
		TriNumber<T> checkVec = { x / length, y / length, z / length };
		if (checkVec == TriNumber<T>{ 0, 0, 0 })
			return { Cardinal::Value::None, Cardinal::Value::None, Cardinal::Value::None };

		if (checkVec.x > 0)
			dirs[dirIndex++] = (Cardinal::Value::East);
		else if (checkVec.x < 0)
			dirs[dirIndex++] = (Cardinal::Value::West);
		if (checkVec.z > 0)
			dirs[dirIndex++] = (Cardinal::Value::South);
		else if (checkVec.z < 0)
			dirs[dirIndex++] = (Cardinal::Value::North);
		if (checkVec.y > 0)
			dirs[dirIndex++] = (Cardinal::Value::Up);
		else if (checkVec.y < 0)
			dirs[dirIndex++] = (Cardinal::Value::Down);
		// NOTE: If dirIndex is ever > 3, someone is
		// trying to make this codebase support 4 or more dimensions.
		// So as long as that doesn't happen,
		// we can probably safely ignore that case.
		// We'll know if we ever get a segfault here :p
		return dirs;
	}
};

/**
 * @brief A struct that contains two numbers (x,y)
 * 
 */
template <typename T = int>
struct BiNumber {
	union {
		// For accessing directly
		struct {
			T x;
			union {
				T y;
				T z;
			};
		};
		// For accessing as array
		T data[2];
	};

	constexpr BiNumber(T _x, T _y) : x(_x), y(_y) {}
	constexpr BiNumber() : x(0), y(0) {}

	bool operator==(const BiNumber& _other) const {
		return x == _other.x && y == _other.y;
	}

	BiNumber operator+(const BiNumber& _other) const {
		return BiNumber{ x + _other.x, y + _other.y };
	}

	BiNumber operator-(const BiNumber& _other) const {
		return BiNumber{ x - _other.x, y - _other.y };
	}

	BiNumber operator*(const BiNumber& _other) const {
		return BiNumber{ x * _other.x, y * _other.y };
	}

	BiNumber operator/(const BiNumber& _other) const {
		return BiNumber{ x / _other.x, y / _other.y };
	}

	BiNumber operator+=(const BiNumber& _other) {
		x += _other.x;
		y += _other.y;
		return *this;
	}

	BiNumber operator-=(const BiNumber& _other) {
		x -= _other.x;
		y -= _other.y;
		return *this;
	}

	BiNumber operator*=(const BiNumber& _other) {
		x *= _other.x;
		y *= _other.y;
		return *this;
	}

	BiNumber operator/=(const BiNumber& _other) {
		x /= _other.x;
		y /= _other.y;
		return *this;
	}

	// Allows for bi + 2 = (x+2,y+2)
	template <typename U>
	auto operator+(const U& _other) const {
		using R = std::common_type_t<T, U>;
		return BiNumber<R>{ static_cast<R>(x) + _other, static_cast<R>(y) + _other };
	}

	// Allows for bi - 2 = (x-2,y-2)
	template <typename U>
	auto operator-(const U& _other) const {
		using R = std::common_type_t<T, U>;
		return BiNumber<R>{
			static_cast<R>(x) - _other,
			static_cast<R>(y) - _other,
		};
	}

	// Allows for bi * 2 = (x*2,y*2)
	template <typename U>
	auto operator*(const U& _other) const {
		using R = std::common_type_t<T, U>;
		return BiNumber<R>{ static_cast<R>(x) * _other, static_cast<R>(y) * _other };
	}

	// Allows for bi / 2 = (x/2,y/2)
	template <typename U>
	auto operator/(const U& _other) const {
		using R = std::common_type_t<T, U>;
		return BiNumber<R>{
			static_cast<R>(x) / _other,
			static_cast<R>(y) / _other,
		};
	}

	template <typename U>
	auto operator-=(const U& _other) {
		using R = std::common_type_t<T, U>;
		x -= static_cast<R>(_other);
		y -= static_cast<R>(_other);
		return *this;
	}

	template <typename U>
	auto operator+=(const U& _other) {
		using R = std::common_type_t<T, U>;
		x += static_cast<R>(_other);
		y += static_cast<R>(_other);
		return *this;
	}

	template <typename U>
	auto operator*=(const U& _other) {
		using R = std::common_type_t<T, U>;
		x *= static_cast<R>(_other);
		y *= static_cast<R>(_other);
		return *this;
	}

	template <typename U>
	auto operator/=(const U& _other) {
		using R = std::common_type_t<T, U>;
		x /= static_cast<R>(_other);
		y /= static_cast<R>(_other);
		return *this;
	}

#ifdef TRINUM_HAS_GLM
	template <typename U, glm::qualifier Q>
	constexpr BiNumber(const glm::vec<2, U, Q>& _v) : x(static_cast<T>(_v.x)), y(static_cast<T>(_v.y)) {}

	template <typename U, glm::qualifier Q>
	operator glm::vec<2, U, Q>() const {
		return glm::vec<2, U, Q>(static_cast<U>(x), static_cast<U>(y));
	}
#endif

	friend std::ostream& operator<<(std::ostream& _os, const BiNumber& _val) {
		if constexpr (std::is_integral_v<T>) {
			_os << "(" << static_cast<int64_t>(_val.x) << ", " << static_cast<int64_t>(_val.y) << ")";
		} else {
			_os << "(" << _val.x << ", " << _val.y << ")";
		}
		return _os;
	}

	std::string Str() const {
		std::ostringstream oss;
		oss << *this; // Use the overloaded << operator
		return oss.str();
	}

	T Total() const {
		return x * y;
	}

	auto Offset(const Cardinal::Value _direction, const T _value = 1) {
		switch (_direction) {
		case Cardinal::Value::None:
			break;
		case Cardinal::Value::Up:
		case Cardinal::Value::North:
			z -= _value;
			break;
		case Cardinal::Value::Down:
		case Cardinal::Value::South:
			z += _value;
			break;
		case Cardinal::Value::East:
			x += _value;
			break;
		case Cardinal::Value::West:
			x -= _value;
			break;
		}
		return *this;
	}

	auto WithOffset(const Cardinal::Value _direction, const T _value = 1) const {
		BiNumber<T> result = *this;
		return result.Offset(_direction, _value);
	}
};

/**
 * @brief A struct that contains three numbers, two 32-bit integers for x and z, and a variable-sized y (8, 16, 32, 64 bits)
 * 
 */
template <typename T = int>
struct SlimInt3 {
	// For accessing directly
	struct {
		int32_t x;
		T y;
		int32_t z;
	};

	constexpr SlimInt3(int32_t _x, T _y, int32_t _z) : x(_x), y(_y), z(_z) {}
	constexpr SlimInt3() : x(0), y(0), z(0) {}

	bool operator==(const SlimInt3& _other) const {
		return x == _other.x && y == _other.y && z == _other.z;
	}

	SlimInt3 operator+(const SlimInt3& _other) const {
		return SlimInt3{ x + _other.x, y + _other.y, z + _other.z };
	}

	SlimInt3 operator-(const SlimInt3& _other) const {
		return SlimInt3{ x - _other.x, y - _other.y, z - _other.z };
	}

	SlimInt3 operator*(const SlimInt3& _other) const {
		return SlimInt3{ x * _other.x, y * _other.y, z * _other.z };
	}

	SlimInt3 operator/(const SlimInt3& _other) const {
		return SlimInt3{ x / _other.x, y / _other.y, z / _other.z };
	}

	// Allows for tri + 1 = (x+1,y+1,z+1)
	template <typename U>
	auto operator-(const U& _other) const {
		using R = std::common_type_t<SlimInt3, U>;
		return SlimInt3{ static_cast<R>(x) - _other, static_cast<R>(y) - _other, static_cast<R>(z) - _other };
	}

	// Allows for tri - 1 = (x-1,y-1,z-1)
	template <typename U>
	auto operator+(const U& _other) const {
		using R = std::common_type_t<SlimInt3, U>;
		return SlimInt3{ static_cast<R>(x) + _other, static_cast<R>(y) + _other, static_cast<R>(z) + _other };
	}

	// Allows for tri * 2 = (x*2,y*2,z*2)
	template <typename U>
	auto operator*(const U& _other) const {
		using R = std::common_type_t<SlimInt3, U>;
		return SlimInt3{ static_cast<R>(x) * _other, static_cast<R>(y) * _other, static_cast<R>(z) * _other };
	}

	// Allows for tri / 2 = (x/2,y/2,z/2)
	template <typename U>
	auto operator/(const U& _other) const {
		using R = std::common_type_t<SlimInt3, U>;
		return SlimInt3{ static_cast<R>(x) / _other, static_cast<R>(y) / _other, static_cast<R>(z) / _other };
	}

	friend std::ostream& operator<<(std::ostream& _os, const SlimInt3& _val) {
		_os << "(" << static_cast<int64_t>(_val.x) << ", " << static_cast<int64_t>(_val.y) << ", "
		    << static_cast<int64_t>(_val.z) << ")";
		return _os;
	}

	std::string Str() const {
		std::ostringstream oss;
		oss << *this; // Use the overloaded << operator
		return oss.str();
	}

	T Total() const {
		return x * y * z;
	}
};

/* --- Pre-defined Tri and Bi numbers --- */

// Vector/Double (64-Bit float)
typedef TriNumber<double> Vec3;
typedef Vec3 Double3;
typedef BiNumber<double> Vec2;
typedef Vec2 Double2;

#define VEC3_ZERO Vec3{ 0.0, 0.0, 0.0 }
#define VEC3_ONE Vec3{ 1.0, 1.0, 1.0 }
#define VEC2_ZERO Vec2{ 0.0, 0.0 }
#define VEC2_ONE Vec2{ 1.0, 1.0 }

#define DOUBLE3_ZERO Double3{ 0.0, 0.0, 0.0 }
#define DOUBLE3_ONE Double3{ 1.0, 1.0, 1.0 }
#define DOUBLE2_ZERO Double2{ 0.0, 0.0 }
#define DOUBLE2_ONE Double2{ 1.0, 1.0 }

// Float (32-Bit float)
typedef TriNumber<float> Float3;
typedef BiNumber<float> Float2;

#define FLOAT3_ZERO Float3{ 0.0f, 0.0f, 0.0f }
#define FLOAT3_ONE Float3{ 1.0f, 1.0f, 1.0f }
#define FLOAT2_ZERO Float2{ 0.0f, 0.0f }
#define FLOAT2_ONE Float2{ 1.0f, 1.0f }

// 8-Bit Integer
typedef TriNumber<int8_t> Int8_3;
typedef BiNumber<int8_t> Int8_2;

#define INT8_3_ZERO Int8_3{ 0, 0, 0 }
#define INT8_3_ONE Int8_3{ 1, 1, 1 }
#define INT8_2_ZERO Int8_2{ 0, 0 }
#define INT8_2_ONE Int8_2{ 1, 1 }

typedef Int8_3 Byte3;
typedef Int8_2 Byte2;

#define BYTE3_ZERO INT8_3_ZERO
#define BYTE3_ONE INT8_3_ONE
#define BYTE2_ZERO INT8_2_ZERO
#define BYTE2_ONE INT8_2_ONE

// 16-Bit Integer
typedef TriNumber<int16_t> Int16_3;
typedef BiNumber<int16_t> Int16_2;

#define INT16_3_ZERO Int16_3{ 0, 0, 0 }
#define INT16_3_ONE Int16_3{ 1, 1, 1 }
#define INT16_2_ZERO Int16_2{ 0, 0 }
#define INT16_2_ONE Int16_2{ 1, 1 }

typedef Int16_3 Short3;
typedef Int16_2 Short2;

#define SHORT3_ZERO INT16_3_ZERO
#define SHORT3_ONE INT16_3_ONE
#define SHORT2_ZERO INT16_2_ZERO
#define SHORT2_ONE INT16_2_ONE

// 32-Bit Integer (default)
typedef TriNumber<int32_t> Int32_3;
typedef BiNumber<int32_t> Int32_2;

#define INT32_3_ZERO Int32_3{ 0, 0, 0 }
#define INT32_3_ONE Int32_3{ 1, 1, 1 }
#define INT32_2_ZERO Int32_2{ 0, 0 }
#define INT32_2_ONE Int32_2{ 1, 1 }

typedef Int32_3 Int3;
typedef Int32_2 Int2;

#define INT3_ZERO INT32_3_ZERO
#define INT3_ONE INT32_3_ONE
#define INT2_ZERO INT32_2_ZERO
#define INT2_ONE INT32_2_ONE

// 64-Bit Integer
typedef TriNumber<int64_t> Int64_3;
typedef BiNumber<int64_t> Int64_2;

#define INT64_3_ZERO Int64_3{ 0, 0, 0 }
#define INT64_3_ONE Int64_3{ 1, 1, 1 }
#define INT64_2_ZERO Int64_2{ 0, 0 }
#define INT64_2_ONE Int64_2{ 1, 1 }

typedef Int64_3 Long3;
typedef Int64_2 Long2;

#define LONG3_ZERO INT64_3_ZERO
#define LONG3_ONE INT64_3_ONE
#define LONG2_ZERO INT64_2_ZERO
#define LONG2_ONE INT64_2_ONE

// Unsigned 8-Bit Integer
typedef TriNumber<uint8_t> UInt8_3;
typedef BiNumber<uint8_t> UInt8_2;

#define UINT8_3_ZERO UInt8_3{ 0, 0, 0 }
#define UINT8_3_ONE UInt8_3{ 1, 1, 1 }
#define UINT8_2_ZERO UInt8_2{ 0, 0 }
#define UINT8_2_ONE UInt8_2{ 1, 1 }

typedef UInt8_3 UByte3;
typedef UInt8_2 UByte2;

#define UBYTE3_ZERO UINT8_3_ZERO
#define UBYTE3_ONE UINT8_3_ONE
#define UBYTE2_ZERO UINT8_2_ZERO
#define UBYTE2_ONE UINT8_2_ONE

// Unsigned 16-Bit Integer
typedef TriNumber<uint16_t> UInt16_3;
typedef BiNumber<uint16_t> UInt16_2;

#define UINT16_3_ZERO UInt16_3{ 0, 0, 0 }
#define UINT16_3_ONE UInt16_3{ 1, 1, 1 }
#define UINT16_2_ZERO UInt16_2{ 0, 0 }
#define UINT16_2_ONE UInt16_2{ 1, 1 }

typedef UInt16_3 UShort3;
typedef UInt16_2 UShort2;

#define USHORT3_ZERO UINT16_3_ZERO
#define USHORT3_ONE UINT16_3_ONE
#define USHORT2_ZERO UINT16_2_ZERO
#define USHORT2_ONE UINT16_2_ONE

// Unsigned 32-Bit Integer (default)
typedef TriNumber<uint32_t> UInt32_3;
typedef BiNumber<uint32_t> UInt32_2;

#define UINT32_3_ZERO UInt32_3{ 0, 0, 0 }
#define UINT32_3_ONE UInt32_3{ 1, 1, 1 }
#define UINT32_2_ZERO UInt32_2{ 0, 0 }
#define UINT32_2_ONE UInt32_2{ 1, 1 }

typedef UInt32_3 UInt3;
typedef UInt32_2 UInt2;

#define UINT3_ZERO UINT32_3_ZERO
#define UINT3_ONE UINT32_3_ONE
#define UINT2_ZERO UINT32_2_ZERO
#define UINT2_ONE UINT32_2_ONE

// Unsigned 64-Bit Integer
typedef TriNumber<uint64_t> UInt64_3;
typedef BiNumber<uint64_t> UInt64_2;

#define UINT64_3_ZERO UInt64_3{ 0, 0, 0 }
#define UINT64_3_ONE UInt64_3{ 1, 1, 1 }
#define UINT64_2_ZERO UInt64_2{ 0, 0 }
#define UINT64_2_ONE UInt64_2{ 1, 1 }

typedef Int64_3 ULong3;
typedef Int64_2 ULong2;

#define ULONG3_ZERO UINT64_3_ZERO
#define ULONG3_ONE UINT64_3_ONE
#define ULONG2_ZERO UINT64_2_ZERO
#define ULONG2_ONE UINT64_2_ONE

// Slim Int3 defines
#define SLIM_INT3_ZERO SlimInt3{ 0, 0, 0 }
#define SLIM_INT3_ONE SlimInt3{ 1, 1, 1 }

// For hashing
namespace std {
template <typename T>
struct hash<TriNumber<T>> {
	size_t operator()(const TriNumber<T>& _p) const noexcept {
		size_t h = hash<T>{}(_p.x);
		h ^= hash<T>{}(_p.y) * 0x9e3779b9u + 0x6b3a9a4fu;
		h ^= hash<T>{}(_p.z) * 0x517cc1b7u + 0x2c62a8d3u;
		return h;
	}
};

template <typename T>
struct hash<BiNumber<T>> {
	size_t operator()(const BiNumber<T>& _p) const noexcept {
		size_t h = hash<T>{}(_p.x);
		h ^= hash<T>{}(_p.y) * 0x9e3779b9u + 0x6b3a9a4fu;
		return h;
	}
};
} // namespace std