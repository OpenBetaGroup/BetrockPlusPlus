/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#pragma once
#include <cstddef>
#include <string>

// Some helper functions to provide reliable
// conversion from and to UTF-8 and UCS-2,
// as it's needed for various things,
// such as NBT and Packet data

std::string ToUTF8(std::u16string _str);
char32_t DecodeUTF8Char(const std::string& _s, size_t& _i);
std::u16string ToUCS2(std::string _str);
inline std::string StripFormatting(const std::string& _str) {
	std::string result;
	result.reserve(_str.size());

	for (size_t i = 0; i < _str.size(); i++) {
		unsigned char c = static_cast<unsigned char>(_str[i]);

		// Formatting code: § followed by one code character
		if (c == 0xA7 && i + 1 < _str.size()) {
			i++;
			continue;
		}

		// Standard printable ASCII
		if (c >= 0x20 && c <= 0x7E) {
			result += static_cast<char>(c);
		}
	}

	return result;
}