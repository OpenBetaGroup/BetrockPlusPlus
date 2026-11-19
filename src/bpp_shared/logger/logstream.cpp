/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#include "logstream.h"
#include "logger.h"
#include <iostream>
#include <memory>

LogStream::LogStream(LogLevel _level) : level(_level) {}

LogStream& LogStream::operator<<(Manip _manip) {
	if (_manip == static_cast<Manip>(std::endl)) {
		Flush();
	} else {
		_manip(buffer);
	}

	return *this;
}

void LogStream::Flush() {
	Logger::Log(buffer.str(), level);

	buffer.str("");
	buffer.clear();
}