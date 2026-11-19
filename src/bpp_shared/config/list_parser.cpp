/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 * 
*/

#include "list_parser.h"
#include "logger.h"
#include "logstream.h"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <string>
#include <utility>

namespace {

std::string Trim(std::string _value) {
	auto notSpace = [](unsigned char _c) {
		return !std::isspace(_c);
	};
	_value.erase(_value.begin(), std::find_if(_value.begin(), _value.end(), notSpace));
	_value.erase(std::find_if(_value.rbegin(), _value.rend(), notSpace).base(), _value.end());
	return _value;
}

} // namespace

std::vector<std::string> ListParser::Read(Target _target) {
	std::vector<std::string> values;
	const std::string path{ TARGET_PATHS[size_t(_target)] };
	std::ifstream file(path);
	if (!file) {
		Logger::Warning << path << " doesn't exist! Creating it..." << "\n";
		std::ofstream createFile(path);
		if (!createFile)
			Logger::Error << "Failed to create " << path << "\n";
		return {};
	}
	for (std::string username; std::getline(file, username);) {
		username = Trim(std::move(username));
		if (username.empty())
			continue;
		if (std::find(values.begin(), values.end(), username) == values.end())
			values.push_back(std::move(username));
	}
	return values;
}

bool ListParser::Write(const std::vector<std::string>& _list, Target _target) {
	const std::string path{ TARGET_PATHS[size_t(_target)] };
	std::ofstream file(path);
	if (!file) {
		Logger::Error << "Failed to open " << path << " for writing\n";
		return false;
	}
	for (const auto& entry : _list)
		file << entry << '\n';
	file.flush();
	if (!file) {
		Logger::Error << "Failed to write " << path << "\n";
		return false;
	}
	return true;
}
