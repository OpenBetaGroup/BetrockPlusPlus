/*
 * Copyright (c) 2025, MINA <github.com/9mina>
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 * 
*/

#include "config.h"

#include "logger.h"
#include "logstream.h"
#include <fstream>
#include <iostream>
#include <mutex>
#include <vector>

// TODO: Replace std::string_view with std::filesystem::path
Config::Config(const std::string& _pPath) {
	this->path = _pPath;
}

std::string_view Config::Get(const std::string& _key) noexcept {
	std::shared_lock readLock{ this->propertiesMutex };
	return this->properties.contains(_key) ? this->properties.at(_key) : std::string_view();
}

void Config::Set(const std::string& _key, std::string_view _value) noexcept {
	std::unique_lock writeLock{ this->propertiesMutex };
	this->properties[_key] = _value;
}

// overwrite the properties in memory
void Config::Overwrite(const ConfType& _config) noexcept {
	std::unique_lock writeLock{ this->propertiesMutex };
	this->properties = _config;
}

bool Config::LoadFromDisk() noexcept {
	std::ifstream file(this->path);

	if (!file.is_open()) {
		Logger::Warning << "**** Error opening properties file (load). Attempting to create new file...\n";
		return false;
	}

	std::unique_lock lock{ this->propertiesMutex };

	this->properties.clear();

	std::string line;
	while (std::getline(file, line)) {
		// Skip empty lines and comments
		if (line.empty() || line[0] == '#')
			continue;

		auto delimiterPos = line.find('=');
		if (delimiterPos == std::string::npos) {
			Logger::Error << "**** Invalid line in properties file: " << line << "\n";
			continue;
		}

		std::string key = line.substr(0, delimiterPos);
		std::string value = line.substr(delimiterPos + 1);

		// Trim whitespace (optional)
		key.erase(key.find_last_not_of(" \t\n\r\f\v") + 1);
		value.erase(0, value.find_first_not_of(" \t\n\r\f\v"));

		properties[key] = value;
	}
	file.close();
	return true;
}

bool Config::SaveToDisk() const noexcept {
	std::ofstream file(this->path);
	if (!file.is_open()) {
		Logger::Error << "**** Error opening properties file (save). \n";
		return false;
	}

	try {
		for (const auto& [key, value] : this->properties) {
			file << key << "=" << value << "\n";
		}
	} catch (const std::exception& e) {
		Logger::Error << "**** Error while writing properties file: " << e.what() << "\n";
		return false;
	}

	Logger::Info << "Properties file saved successfully.\n";
	file.close();
	return true;
}

bool Config::SaveKeyToDisk(const std::string& _key) {
	std::string value;
	{
		std::shared_lock lock{ this->propertiesMutex };
		auto it = this->properties.find(_key);
		if (it == this->properties.end())
			return false;
		value = it->second;
	}

	std::ifstream in(this->path);
	if (!in.is_open())
		return SaveToDisk();

	std::vector<std::string> lines;
	bool found = false;
	std::string line;
	while (std::getline(in, line)) {
		if (!line.empty() && line[0] != '#') {
			auto delimiterPos = line.find('=');
			if (delimiterPos != std::string::npos) {
				std::string key = line.substr(0, delimiterPos);
				key.erase(key.find_last_not_of(" \t\n\r\f\v") + 1);
				if (key == _key) {
					line = _key + "=" + value;
					found = true;
				}
			}
		}
		lines.push_back(std::move(line));
	}
	in.close();
	if (!found)
		lines.push_back(_key + "=" + value);

	std::ofstream out(this->path);
	if (!out.is_open()) {
		Logger::Error << "**** Error opening properties file (save key).\n";
		return false;
	}
	for (const auto& l : lines)
		out << l << '\n';
	if (!out) {
		Logger::Error << "**** Error while writing properties file\n";
		return false;
	}
	return true;
}

void Config::SetPath(std::string_view _pPath) noexcept {
	this->path = _pPath;
}

std::string_view Config::GetPath() const noexcept {
	return this->path;
}