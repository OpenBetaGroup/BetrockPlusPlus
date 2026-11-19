/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once
#include "logger.h"
#include "logstream.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

class FileHandle {
public:
	FileHandle() = default;
	FileHandle(const std::string& _path) : stream(_path, std::ios::in | std::ios::out | std::ios::binary) {
		if (!stream.is_open()) {
			Logger::Info << "Failed to open file: " << _path << "\n";
			throw std::runtime_error("Failed to open file: " + _path);
		}
	}
	FileHandle(std::filesystem::path& _path) : stream(_path, std::ios::in | std::ios::out | std::ios::binary) {
		if (!stream.is_open()) {
			Logger::Info << "Failed to open file: " << _path.string() << "\n";
			throw std::runtime_error("Failed to open file: " + _path.string());
		}
	}
	std::fstream& Get() {
		return stream;
	}

private:
	std::fstream stream;
};