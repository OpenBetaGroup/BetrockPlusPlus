/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * 
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

#include "gamerules.h"

// Vanilla b1.7.3 treats usernames case-insensitively (bans, ops, whitelist, commands).
// The respect_username_capitalization gamerule (default false) restores exact matching.
inline bool UsernameEquals(std::string_view _a, std::string_view _b) {
	if (Gamerule::Get<bool>(Gamerule::Id::RespectUsernameCapitalization))
		return _a == _b;
	return std::equal(_a.begin(), _a.end(), _b.begin(), _b.end(),
	                  [](unsigned char x, unsigned char y) { return std::tolower(x) == std::tolower(y); });
}

inline std::string NormalizeUsername(std::string _name) {
	if (!Gamerule::Get<bool>(Gamerule::Id::RespectUsernameCapitalization))
		std::transform(_name.begin(), _name.end(), _name.begin(), [](unsigned char c) { return std::tolower(c); });
	return _name;
}

inline std::vector<std::string>::iterator FindUsername(std::vector<std::string>& _list, std::string_view _name) {
	return std::find_if(_list.begin(), _list.end(), [&](const std::string& _s) { return UsernameEquals(_s, _name); });
}

inline bool ContainsUsername(const std::vector<std::string>& _list, std::string_view _name) {
	return std::any_of(_list.begin(), _list.end(), [&](const std::string& _s) { return UsernameEquals(_s, _name); });
}

inline bool IsValidUsername(const std::string& _username, bool _allowDotPrefix = false) {
	if (_username.size() < 3 || _username.size() > 16)
		return false;
	size_t index = 0;
	for (unsigned char c : _username) {
		const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
		                (index == 0 && c == '.' && _allowDotPrefix);
		index++;
		if (!ok)
			return false;
	}
	return true;
}
