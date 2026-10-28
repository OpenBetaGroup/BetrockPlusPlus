/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once
#include "helpers/file_handle.h"
#include "java_math.h"
#include "nbt/nbt.h"
#include "platform/fs_compat.h"
#include "platform/platform_caps.h"
#include "utilities.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include "helpers/deflate_pool.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#else
#include <fcntl.h>
#ifdef BPP_HAS_FLOCK
#include <sys/file.h>
#endif
#include <unistd.h>
#endif

enum class LevelInitFailureReason : uint8_t {
	SUCCESS,
	ALPHA_FORMATTED,
	NEW_SAVE,
	MISSING_FOLDERS,
	INVALID_LOCK,
	LEVELDAT_FAILURE
};

struct LevelData {
	int64_t randomSeed = 0;
	Int3 spawnPoint{ 0, 0, 0 };
	int rainTime = 0;
	int thunderTime = 0;
	int8_t raining = 0;
	int64_t time = 0;
	int8_t thundering = 0;
	int version = 19132;
	int64_t lastPlayed = 0;
	std::string levelName = "world";
	int64_t sizeOnDisk = 0;
};

struct SessionLock {
#ifdef _WIN32
	HANDLE handle = INVALID_HANDLE_VALUE;
#else
	int fd = -1;
#endif

	bool Acquire(const std::string& _path) {
#ifdef _WIN32
		handle = CreateFileA(_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
		                     FILE_ATTRIBUTE_NORMAL, nullptr);
		if (handle == INVALID_HANDLE_VALUE)
			return false;
#else
		fd = open(_path.c_str(), O_RDWR | O_CREAT, 0644);
		if (fd < 0)
			return false;
#ifdef BPP_HAS_FLOCK
		if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
			close(fd);
			fd = -1;
			return false;
		}
#endif // Consoles have no flock(); there's only ever one process there
#endif
		WriteTimestamp();
		return true;
	}

	void Release() {
#ifdef _WIN32
		if (handle != INVALID_HANDLE_VALUE) {
			CloseHandle(handle);
			handle = INVALID_HANDLE_VALUE;
		}
#else
		if (fd >= 0) {
#ifdef BPP_HAS_FLOCK
			flock(fd, LOCK_UN);
#endif
			close(fd);
			fd = -1;
		}
#endif
	}

	~SessionLock() {
		Release();
	}

private:
	void WriteTimestamp() {
		int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
		                  std::chrono::system_clock::now().time_since_epoch())
		                  .count();
		uint8_t bytes[8];
		for (int i = 7; i >= 0; i--) {
			bytes[i] = now & 0xFF;
			now >>= 8;
		}
#ifdef _WIN32
		DWORD written;
		WriteFile(handle, bytes, 8, &written, nullptr);
#else
		write(fd, bytes, 8);
#endif
	}
};

struct SaveManager {
	LevelInitFailureReason Initialize(const std::string& _pSaveName, bool /*_isMultiplayerSave*/ = false) {
		saveDirectory = _pSaveName;

		// Make sure session lock and level.dat are valid
		if (!sessionLock.Acquire(saveDirectory + "/session.lock"))
			return LevelInitFailureReason::INVALID_LOCK;
		if (!std::filesystem::exists(saveDirectory + "/level.dat")) {
			Logger::Warning << "level.dat couldn't be found!\n";
			if (!std::filesystem::exists(saveDirectory + "/level.dat_old")) {
				Logger::Warning << "level.dat_old couldn't be found!\n";
				return LevelInitFailureReason::LEVELDAT_FAILURE;
			}
			if (!Platform::FS::CopyFileOverwrite(saveDirectory + "/level.dat_old", saveDirectory + "/level.dat"))
				return LevelInitFailureReason::LEVELDAT_FAILURE;
		}
		worldFile = std::make_unique<FileHandle>(saveDirectory + "/level.dat");
		if (!worldFile->Get().is_open())
			return LevelInitFailureReason::LEVELDAT_FAILURE;

		// See if we are an alpha level
		if (Utilities::isAlphaLevel(_pSaveName)) {
			return LevelInitFailureReason::ALPHA_FORMATTED;
		}

		// Check for missing directories
		int necessaryFolders = 0;
		necessaryFolders += std::filesystem::create_directories(saveDirectory + "/players");
		necessaryFolders += std::filesystem::create_directories(saveDirectory + "/region");
		necessaryFolders += std::filesystem::create_directories(saveDirectory + "/DIM-1/region");
		necessaryFolders += std::filesystem::create_directories(saveDirectory + "/data");
		if (necessaryFolders)
			Logger::Warning << "Failed to load " << necessaryFolders
			                    << " necessary folder(s) for level " + _pSaveName + ".\n";

		return LevelInitFailureReason::SUCCESS;
	}

	bool LoadLevelData() {
		if (!worldFile || !worldFile->Get().is_open())
			return false;

		std::fstream& stream = worldFile->Get();

		// Read entire file into buffer
		stream.seekg(0, std::ios::end);
		size_t fileSize = stream.tellg();
		stream.seekg(0, std::ios::beg);
		std::vector<uint8_t> compressed(fileSize);
		stream.read(reinterpret_cast<char*>(compressed.data()), fileSize);

		// Decompress gzip
		std::vector<uint8_t> raw(1024); // 1KB should be plenty for level.dat, unless the world name is SUPER huge
		size_t actualSize = 0;
		if (!Deflate::Decompress(Deflate::Format::Gzip, compressed, raw, actualSize))
			return false;
		raw.resize(actualSize);

		// Parse NBT
		NBTParser parser(raw.data(), raw.size());
		Tag data = parser.root.Get("Data");

		auto CheckAndAssignDefault = [&](std::string tagName, TagType type, auto defaultValue) {
			if (!data.compound.count(tagName)) {
				Logger::Warning << "Tag " << tagName << " not found in level.dat!\n";
				Tag tag{ .type = type, .name = tagName };
				tag.Set(defaultValue);
				data.compound[tagName] = tag;
			}
		};

		// Check to make sure we have valid tags
		CheckAndAssignDefault("RandomSeed", TAG_LONG, 0);
		CheckAndAssignDefault("SpawnX", TAG_INT, 0);
		CheckAndAssignDefault("SpawnY", TAG_INT, 64);
		CheckAndAssignDefault("SpawnZ", TAG_INT, 0);
		CheckAndAssignDefault("rainTime", TAG_INT, 0);
		CheckAndAssignDefault("thunderTime", TAG_INT, 0);
		CheckAndAssignDefault("raining", TAG_BYTE, 0);
		CheckAndAssignDefault("Time", TAG_LONG, 0);
		CheckAndAssignDefault("thundering", TAG_BYTE, 0);
		CheckAndAssignDefault("version", TAG_INT, 19132);
		CheckAndAssignDefault("LastPlayed", TAG_LONG, 0);
		CheckAndAssignDefault("LevelName", TAG_STRING, "world");
		CheckAndAssignDefault("SizeOnDisk", TAG_LONG, 0);

		currentLevelData.randomSeed = data.Get("RandomSeed").longValue;
		currentLevelData.spawnPoint.x = data.Get("SpawnX").intValue;
		currentLevelData.spawnPoint.y = data.Get("SpawnY").intValue;
		currentLevelData.spawnPoint.z = data.Get("SpawnZ").intValue;
		currentLevelData.rainTime = data.Get("rainTime").intValue;
		currentLevelData.thunderTime = data.Get("thunderTime").intValue;
		currentLevelData.raining = data.Get("raining").byteValue;
		currentLevelData.time = data.Get("Time").longValue;
		currentLevelData.thundering = data.Get("thundering").byteValue;
		currentLevelData.version = data.Get("version").intValue;
		currentLevelData.lastPlayed = data.Get("LastPlayed").longValue;
		currentLevelData.levelName = data.Get("LevelName").stringValue;
		currentLevelData.sizeOnDisk = data.Get("SizeOnDisk").longValue;

		return true;
	}

	LevelData& GetLevelData() {
		return currentLevelData;
	}

	bool CreateNewWorld(const LevelData& _data) {
		std::error_code ec;
		std::filesystem::create_directories(saveDirectory + "/players", ec);
		std::filesystem::create_directories(saveDirectory + "/region", ec);
		std::filesystem::create_directories(saveDirectory + "/DIM-1/region", ec);
		std::filesystem::create_directories(saveDirectory + "/data", ec);
		return SaveLevelFile(_data);
	}

	bool SaveLevelFile(const LevelData& _levelData) {
		// Back up existing level.dat if present
		if (std::filesystem::exists(saveDirectory + "/level.dat")) {
			if (!Platform::FS::CopyFileOverwrite(saveDirectory + "/level.dat", saveDirectory + "/level.dat_old"))
				Logger::Warning << "Couldn't back up level.dat to level.dat_old.\n";
		}

		Tag root;
		root.type = TAG_COMPOUND;
		root.name = "";

		Tag data;
		data.type = TAG_COMPOUND;
		data.name = "Data";

		Tag randomSeed;
		randomSeed.type = TAG_LONG;
		randomSeed.name = "RandomSeed";
		randomSeed.longValue = _levelData.randomSeed;
		Tag spawnX;
		spawnX.type = TAG_INT;
		spawnX.name = "SpawnX";
		spawnX.intValue = _levelData.spawnPoint.x;
		Tag spawnY;
		spawnY.type = TAG_INT;
		spawnY.name = "SpawnY";
		spawnY.intValue = _levelData.spawnPoint.y;
		Tag spawnZ;
		spawnZ.type = TAG_INT;
		spawnZ.name = "SpawnZ";
		spawnZ.intValue = _levelData.spawnPoint.z;
		Tag rainTime;
		rainTime.type = TAG_INT;
		rainTime.name = "rainTime";
		rainTime.intValue = _levelData.rainTime;
		Tag thunderTime;
		thunderTime.type = TAG_INT;
		thunderTime.name = "thunderTime";
		thunderTime.intValue = _levelData.thunderTime;
		Tag raining;
		raining.type = TAG_BYTE;
		raining.name = "raining";
		raining.byteValue = _levelData.raining;
		Tag time;
		time.type = TAG_LONG;
		time.name = "Time";
		time.longValue = _levelData.time;
		Tag thundering;
		thundering.type = TAG_BYTE;
		thundering.name = "thundering";
		thundering.byteValue = _levelData.thundering;
		Tag version;
		version.type = TAG_INT;
		version.name = "version";
		version.intValue = _levelData.version;
		Tag lastPlayed;
		lastPlayed.type = TAG_LONG;
		lastPlayed.name = "LastPlayed";
		lastPlayed.longValue = _levelData.lastPlayed;
		Tag levelName;
		levelName.type = TAG_STRING;
		levelName.name = "LevelName";
		levelName.stringValue = _levelData.levelName;
		Tag sizeOnDisk;
		sizeOnDisk.type = TAG_LONG;
		sizeOnDisk.name = "SizeOnDisk";
		sizeOnDisk.longValue = _levelData.sizeOnDisk;

		data.compound["RandomSeed"] = randomSeed;
		data.compound["SpawnX"] = spawnX;
		data.compound["SpawnY"] = spawnY;
		data.compound["SpawnZ"] = spawnZ;
		data.compound["rainTime"] = rainTime;
		data.compound["thunderTime"] = thunderTime;
		data.compound["raining"] = raining;
		data.compound["Time"] = time;
		data.compound["thundering"] = thundering;
		data.compound["version"] = version;
		data.compound["LastPlayed"] = lastPlayed;
		data.compound["LevelName"] = levelName;
		data.compound["SizeOnDisk"] = sizeOnDisk;
		root.compound["Data"] = data;

		std::vector<uint8_t> raw;
		NBTwriter writer(raw, root);

		std::vector<uint8_t> compressed(Deflate::CompressBound(Deflate::Format::Zlib, raw.size()));
		size_t writtenSize = 0;
		if (!Deflate::Compress(Deflate::Format::Gzip, raw, compressed, writtenSize))
			return false;

		std::ofstream file(saveDirectory + "/level.dat", std::ios::binary);
		if (!file.is_open())
			return false;
		file.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());
		if (!file.good())
			return false;

		// Reopen as FileHandle for future use
		worldFile = std::make_unique<FileHandle>(saveDirectory + "/level.dat");
		return true;
	}

	static int64_t SeedFromString(const std::string& _input) {
		// If it's a plain number, use it directly
		try {
			size_t pos;
			int64_t numeric = std::stoll(_input, &pos);
			if (pos == _input.size())
				return numeric;
		} catch (...) {
		}

		// Otherwise hash it
		return HashCode(_input);
	}

	Tag GetPlayerNbt(const std::string& _playerName) { // return by value
		std::string playerPath = saveDirectory + "/players/" + _playerName + ".dat";

		if (!std::filesystem::exists(playerPath)) {
			Tag fresh = GetNewPlayerNbt();
			SavePlayerNbt(_playerName, fresh);
			return fresh;
		}

		// Load existing player file
		std::ifstream file(playerPath, std::ios::binary);
		if (!file.is_open())
			return GetNewPlayerNbt();

		std::vector<uint8_t> compressed((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

		// NOTE: 1MB is way too huge of a guess...
		// Player inventories mostly get grown by holding items, so they have a finite size!
		// TODO: Calculate a reasonable maximum!
		std::vector<uint8_t> raw(1024 * 1024);
		size_t writtenSize = 0;
		if (!Deflate::Decompress(Deflate::Format::Gzip, compressed, raw, writtenSize))
			return GetNewPlayerNbt();

		raw.resize(writtenSize);
		NBTParser parser(raw.data(), raw.size());
		return parser.root;
	}

	Tag GetNewPlayerNbt() {
		Tag root;
		root.type = TAG_COMPOUND;
		root.name = "";

		Tag motion;
		motion.type = TAG_LIST;
		motion.name = "Motion";
		motion.listType = TAG_DOUBLE;
		Tag sleepTimer;
		sleepTimer.type = TAG_SHORT;
		sleepTimer.name = "SleepTimer";
		sleepTimer.shortValue = 0;
		Tag health;
		health.type = TAG_SHORT;
		health.name = "Health";
		health.shortValue = 20;
		Tag air;
		air.type = TAG_SHORT;
		air.name = "Air";
		air.shortValue = 300;
		Tag onGround;
		onGround.type = TAG_BYTE;
		onGround.name = "OnGround";
		onGround.byteValue = 0;
		Tag dimension;
		dimension.type = TAG_INT;
		dimension.name = "Dimension";
		dimension.intValue = 0;
		Tag rotation;
		rotation.type = TAG_LIST;
		rotation.name = "Rotation";
		rotation.listType = TAG_FLOAT;
		Tag fallDistance;
		fallDistance.type = TAG_FLOAT;
		fallDistance.name = "FallDistance";
		fallDistance.floatValue = 0.0f;
		Tag sleeping;
		sleeping.type = TAG_BYTE;
		sleeping.name = "Sleeping";
		sleeping.byteValue = 0;
		Tag pos;
		pos.type = TAG_LIST;
		pos.name = "Pos";
		pos.listType = TAG_DOUBLE;
		Tag deathTime;
		deathTime.type = TAG_SHORT;
		deathTime.name = "DeathTime";
		deathTime.shortValue = 0;
		Tag fire;
		fire.type = TAG_SHORT;
		fire.name = "Fire";
		fire.shortValue = -20;
		Tag hurtTime;
		hurtTime.type = TAG_SHORT;
		hurtTime.name = "HurtTime";
		hurtTime.shortValue = 0;
		Tag attackTime;
		attackTime.type = TAG_SHORT;
		attackTime.name = "AttackTime";
		attackTime.shortValue = 0;
		Tag inventory;
		inventory.type = TAG_LIST;
		inventory.name = "Inventory";
		inventory.listType = TAG_COMPOUND;

		// Initialize our position with a default
		Tag posX;
		posX.type = TAG_DOUBLE;
		posX.doubleValue = -1;
		Tag posY;
		posY.type = TAG_DOUBLE;
		posY.doubleValue = -1000000;
		Tag posZ;
		posZ.type = TAG_DOUBLE;
		posZ.doubleValue = -1;
		pos.list.push_back(posX);
		pos.list.push_back(posY);
		pos.list.push_back(posZ);

		Tag rotX;
		rotX.type = TAG_FLOAT;
		rotX.floatValue = 0.0f;
		Tag rotY;
		rotY.type = TAG_FLOAT;
		rotY.floatValue = 0.0f;
		rotation.list.push_back(rotX);
		rotation.list.push_back(rotY);

		// Initialize our velocity with a default
		Tag movX;
		movX.type = TAG_DOUBLE;
		movX.doubleValue = 0.0;
		Tag movY;
		movY.type = TAG_DOUBLE;
		movY.doubleValue = 0.0;
		Tag movZ;
		movZ.type = TAG_DOUBLE;
		movZ.doubleValue = 0.0;
		motion.list.push_back(movX);
		motion.list.push_back(movY);
		motion.list.push_back(movZ);

		root.compound["Motion"] = motion;
		root.compound["SleepTimer"] = sleepTimer;
		root.compound["Health"] = health;
		root.compound["Air"] = air;
		root.compound["OnGround"] = onGround;
		root.compound["Dimension"] = dimension;
		root.compound["Rotation"] = rotation;
		root.compound["FallDistance"] = fallDistance;
		root.compound["Sleeping"] = sleeping;
		root.compound["Pos"] = pos;
		root.compound["DeathTime"] = deathTime;
		root.compound["Fire"] = fire;
		root.compound["HurtTime"] = hurtTime;
		root.compound["AttackTime"] = attackTime;
		root.compound["Inventory"] = inventory;
		return root;
	}

	bool SavePlayerNbt(const std::string& _playerName, Tag& _playerData) const {
		try {
			// Check to see if we have a root compound tag
			if (_playerData.type != TAG_COMPOUND) {
				return false;
			}
			std::vector<uint8_t> raw;
			NBTwriter writer(raw, _playerData);

			std::vector<uint8_t> compressed(Deflate::CompressBound(Deflate::Format::Gzip, raw.size()));
			size_t writtenSize = 0;
			if (!Deflate::Compress(Deflate::Format::Gzip, raw, compressed, writtenSize, 6))
				return false;
			compressed.resize(writtenSize);

			std::string finalPath = saveDirectory + "/players/" + _playerName + ".dat";
			std::string tmpPath = finalPath + ".tmp";

			std::ofstream file(tmpPath, std::ios::binary);
			if (!file.is_open())
				return false;
			file.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());
			file.flush();
			if (!file.good())
				return false;
			file.close();

			if (!Platform::FS::ReplaceFile(tmpPath, finalPath)) {
				Logger::Error << "Failed to move " << tmpPath << " into place.\n";
				return false;
			}
			return true;
		} catch (const std::exception& e) {
			Logger::Error << "Failed to save player NBT for " << _playerName << ": " << e.what() << "\n";
		} catch (...) {
			Logger::Error << "Failed to save player NBT for " << _playerName << ": unknown exception\n";
		}
		return false;
	}

	void Release() {
		sessionLock.Release();
		worldFile.reset();
	}

	~SaveManager() {
		Release();
	}

private:
	std::string saveDirectory;
	std::unique_ptr<FileHandle> worldFile;
	SessionLock sessionLock;
	LevelData currentLevelData;
};