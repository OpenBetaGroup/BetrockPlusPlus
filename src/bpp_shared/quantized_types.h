/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

// Can be used to simplify fixed-point math for stuff like entities
// TODO: See if we should use this/need this?
// Realistically it shouldn't be used for anything but networked entities,
// whose positions we turn into doubles for internal handling anyways

#pragma once

#include "helpers/endian_convert.h"
#include "numeric_structs.h"
#include <cstdint>
#include <ostream>
#include <type_traits>

template <typename Storage = int32_t, int Scale = 32>
struct Fixed {
	static_assert(std::is_integral_v<Storage>, "Storage must be an integer type");

	using StorageType = Storage;

	static constexpr Storage M_SCALE = Scale;

	Storage mValue;

	constexpr Fixed() : mValue(0) {}

	constexpr Fixed(const Fixed&) = default;
	constexpr Fixed& operator=(const Fixed&) = default;

	template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
	constexpr Fixed(T _mValue) : mValue(static_cast<Storage>(_mValue * M_SCALE)) {}

	static constexpr Fixed FromRaw(Storage _raw) {
		Fixed result;
		result.mValue = _raw;
		return result;
	}

	constexpr Storage Raw() const {
		return mValue;
	}

	constexpr double Value() const {
		return static_cast<double>(mValue) / M_SCALE;
	}

	constexpr explicit operator double() const {
		return Value();
	}

	constexpr explicit operator float() const {
		return static_cast<float>(Value());
	}

	constexpr Fixed operator+(Fixed _other) const {
		return FromRaw(mValue + _other.mValue);
	}

	constexpr Fixed operator-(Fixed _other) const {
		return FromRaw(mValue - _other.mValue);
	}

	constexpr Fixed operator-() const {
		return FromRaw(-mValue);
	}

	constexpr Fixed& operator+=(Fixed _other) {
		mValue += _other.mValue;
		return *this;
	}

	constexpr Fixed& operator-=(Fixed _other) {
		mValue -= _other.mValue;
		return *this;
	}

	template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
	friend constexpr Fixed operator+(T _lhs, Fixed _rhs) {
		return Fixed(_lhs) + _rhs;
	}

	template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
	friend constexpr Fixed operator-(T _lhs, Fixed _rhs) {
		return Fixed(_lhs) - _rhs;
	}

	template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
	friend constexpr Fixed operator*(T _lhs, Fixed _rhs) {
		return Fixed(_lhs) * _rhs;
	}

	template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
	friend constexpr Fixed operator/(T _lhs, Fixed _rhs) {
		return Fixed(_lhs) / _rhs;
	}

	template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
	friend constexpr bool operator<(T _lhs, Fixed _rhs) {
		return Fixed(_lhs) < _rhs;
	}

	template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
	friend constexpr bool operator>(T _lhs, Fixed _rhs) {
		return Fixed(_lhs) > _rhs;
	}

	template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
	friend constexpr bool operator<=(T _lhs, Fixed _rhs) {
		return Fixed(_lhs) <= _rhs;
	}

	template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
	friend constexpr bool operator>=(T _lhs, Fixed _rhs) {
		return Fixed(_lhs) >= _rhs;
	}

	constexpr Fixed operator*(Fixed _other) const {
		using Wide = std::conditional_t<sizeof(Storage) <= 4, int64_t,
#ifdef __SIZEOF_INT128__
		                                __int128_t
#else
		                                int64_t
#endif
		                                >;

		return FromRaw(static_cast<Storage>((static_cast<Wide>(mValue) * static_cast<Wide>(_other.mValue)) / M_SCALE));
	}

	constexpr Fixed operator/(Fixed _other) const {
		using Wide = std::conditional_t<sizeof(Storage) <= 4, int64_t,
#ifdef __SIZEOF_INT128__
		                                __int128_t
#else
		                                int64_t
#endif
		                                >;

		return FromRaw(static_cast<Storage>((static_cast<Wide>(mValue) * M_SCALE) / static_cast<Wide>(_other.mValue)));
	}

	constexpr Fixed& operator*=(Fixed _other) {
		*this = *this * _other;
		return *this;
	}

	constexpr Fixed& operator/=(Fixed _other) {
		*this = *this / _other;
		return *this;
	}

	constexpr bool operator==(Fixed _other) const {
		return mValue == _other.mValue;
	}

	constexpr bool operator!=(Fixed _other) const {
		return mValue != _other.mValue;
	}

	constexpr bool operator<(Fixed _other) const {
		return mValue < _other.mValue;
	}

	constexpr bool operator>(Fixed _other) const {
		return mValue > _other.mValue;
	}

	constexpr bool operator<=(Fixed _other) const {
		return mValue <= _other.mValue;
	}

	constexpr bool operator>=(Fixed _other) const {
		return mValue >= _other.mValue;
	}

	friend std::ostream& operator<<(std::ostream& _os, Fixed _n) {
		return _os << _n.Value();
	}
};

typedef TriNumber<Fixed<int8_t, 32>> NetworkEntityOffset;
typedef TriNumber<Fixed<int32_t, 32>> NetworkEntityPosition;
typedef TriNumber<Fixed<int16_t, 8000>> NetworkEntityVelocity;