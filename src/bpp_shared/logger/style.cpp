/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#include "style.h"
#include <cstddef>

// Translate color codes into ASCII Escape sequence colors
std::string FormatToStyle(int8_t _format) {
	switch (_format) {
	// Colors
	case '0':
		return "\033[30m";
	case '1':
		return "\033[34m";
	case '2':
		return "\033[32m";
	case '3':
		return "\033[36m";
	case '4':
		return "\033[31m";
	case '5':
		return "\033[35m";
	case '6':
		return "\033[33m";
	case '7':
		return "\033[37m";
	case '8':
		return "\033[90m";
	case '9':
		return "\033[94m";
	case 'a':
		return "\033[92m";
	case 'b':
		return "\033[96m";
	case 'c':
		return "\033[91m";
	case 'd':
		return "\033[95m";
	case 'e':
		return "\033[93m";
	case 'f':
		return "\033[97m";
	// Bold
	case 'l':
		return "\033[1m";
	// Strikethrough
	case 'm':
		return "\033[9m";
	// Underlined
	case 'n':
		return "\033[4m";
	// Italic
	case 'o':
		return "\033[3m";
	// Obfuscated
	case 'k':
		return "\033[37;105m";
	// Reset
	default:
		return STYLE_RESET;
	}
}

// Translate the passed string with style formatters into ASCII Escape sequence colors
std::string HandleFormattingCodes(const std::string& _input) {
	std::string output;
	output.reserve(_input.size() + 16);
	for (size_t i = 0; i < _input.size(); ++i) {
		// Check for § (UTF-8: C2 A7) followed by a code character
		if (i + 2 < _input.size() && _input[i] == '\xC2' && _input[i + 1] == '\xA7') {
			const char code = _input[i + 2];
			const bool isColor = (code >= '0' && code <= '9') || (code >= 'a' && code <= 'f');
			if (isColor)
				output += STYLE_RESET;
			output += FormatToStyle(code);
			i += 2; // Skip § and the code character
		} else {
			output += _input[i];
		}
	}
	return output + std::string(STYLE_RESET);
}
