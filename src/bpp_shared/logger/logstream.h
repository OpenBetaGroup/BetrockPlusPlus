/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once

#include "loglevel.h"
#include <sstream>
#include <string>
#include <string_view>

class LogStream {
private:
	LogLevel level;
	std::stringstream buffer;

public:
	LogStream(LogLevel _level);

	template <typename T>
	LogStream& operator<<(const T& _value) {
		buffer << _value;
		return *this;
	}

	LogStream& operator<<(const std::string& _value) {
		return operator<<(_value.c_str());
	}

	LogStream& operator<<(const char* _value) {
		std::string_view sv(_value);
		if (!sv.empty() && sv.back() == '\n') {
			buffer << sv.substr(0, sv.size() - 1);
			Flush();
		} else {
			buffer << _value;
		}
		return *this;
	}

	using Manip = std::ostream& (*)(std::ostream&);

	LogStream& operator<<(Manip _manip);

	void Flush();
};