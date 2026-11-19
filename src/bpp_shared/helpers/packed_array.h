/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

template <std::size_t Count, unsigned Bits>
struct PackedArray {
	static_assert(8 % Bits == 0, "Bits must divide 8 evenly (1, 2, 4, or 8)");
	static constexpr unsigned PER_BYTE = 8 / Bits;
	static constexpr uint8_t MASK = uint8_t((1u << Bits) - 1u);

	std::array<uint8_t, (Count + PER_BYTE - 1) / PER_BYTE> data{};

	uint8_t Get(std::size_t _i) const {
		unsigned shift = (_i % PER_BYTE) * Bits;
		return (data[_i / PER_BYTE] >> shift) & MASK;
	}
	void Set(std::size_t _i, uint8_t _val) {
		unsigned shift = (_i % PER_BYTE) * Bits;
		uint8_t& byte = data[_i / PER_BYTE];
		byte = uint8_t((byte & ~(MASK << shift)) | ((_val & MASK) << shift));
	}
};