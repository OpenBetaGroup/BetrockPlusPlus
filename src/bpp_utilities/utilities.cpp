/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#include "utilities.h"
#include "blocks/block_properties_behaviors.h"
#include "helpers/file_handle.h"
#include "helpers/deflate_pool.h"
#include "nbt/nbt.h"
#include "world/storage/region_manager.h"
#include "world/storage/save_manager.h"
#include "world/world.h"

namespace fs = std::filesystem;

namespace Utilities {

// Creates a temp directory and deletes it if it already exists. Returns false on failure.
bool recreateTempDir(const fs::path& _dir) {
	std::error_code ec;
	fs::remove_all(_dir, ec);
	if (ec) {
		Logger::Error << "Failed to remove directory: " << ec.message() << '\n';
		return false;
	}
	fs::create_directories(_dir, ec);
	if (ec) {
		Logger::Error << "Failed to create directory: " << ec.message() << '\n';
		return false;
	}
	return true;
}

bool isAlphaLevel(const std::string& _dir) {
	// Are we a chunk?
	auto isChunkFilename = [](const std::string& _filename) -> bool {
		static const std::string PREFIX = "c.";
		static const std::string SUFFIX = ".dat";

		if (_filename.size() < PREFIX.size() + SUFFIX.size())
			return false;
		if (_filename.compare(0, PREFIX.size(), PREFIX) != 0)
			return false;
		if (_filename.compare(_filename.size() - SUFFIX.size(), SUFFIX.size(), SUFFIX) != 0)
			return false;

		return true;
	};

	if (!fs::exists(_dir) || !fs::is_directory(_dir))
		return false;

	std::vector<fs::path> result;
	std::error_code ec;

	// skip_permission_denied keeps a single unreadable folder from aborting
	auto it = fs::recursive_directory_iterator(_dir, fs::directory_options::skip_permission_denied, ec);
	auto end = fs::recursive_directory_iterator();

	// Collect chunk paths
	for (; it != end; it.increment(ec)) {
		if (ec) {
			// A single entry failed
			ec.clear();
			continue;
		}

		const fs::directory_entry& entry = *it;
		if (!entry.is_regular_file(ec))
			continue;

		std::string filename = entry.path().filename().string();
		if (isChunkFilename(filename)) {
			result.push_back(entry.path());
		}
	}

	return (result.size() > 0);
}

bool convertAlphaLevel(std::string& _dir) {
	auto decompressChunk = [](const char* _compressedData, size_t _compressedSize, size_t& _decompressedSize) -> std::unique_ptr<char[]> {
		// TODO: Where does 81920 come from??? Who wrote this? Keeping it just because it seems to not have fucked up thus far...
		auto r = Deflate::Decompress(
			Deflate::Format::Gzip,
			{ reinterpret_cast<const uint8_t*>(_compressedData), _compressedSize },
			_decompressedSize ? _decompressedSize : 81920
		);
		if (!r) return nullptr;
		auto out = std::make_unique<char[]>(r->size());
		std::memcpy(out.get(), r->data(), r->size());
		_decompressedSize = r->size();
		return out;
	};

	// Are we a chunk?
	auto isChunkFilename = [](const std::string& _filename) -> bool {
		static const std::string PREFIX = "c.";
		static const std::string SUFFIX = ".dat";

		if (_filename.size() < PREFIX.size() + SUFFIX.size())
			return false;
		if (_filename.compare(0, PREFIX.size(), PREFIX) != 0)
			return false;
		if (_filename.compare(_filename.size() - SUFFIX.size(), SUFFIX.size(), SUFFIX) != 0)
			return false;

		return true;
	};

	// Check if we are in the nether
	auto isInNetherSubtree = [](const fs::path& _worldDir, const fs::path& _chunkFilePath) -> bool {
		std::error_code ec;
		fs::path relative = fs::relative(_chunkFilePath, _worldDir, ec);
		if (ec)
			return false;

		for (const auto& part : relative)
			if (part == "DIM-1")
				return true;

		return false;
	};

	auto registerChunk = [&](WorldManager& _world, std::shared_ptr<Chunk> _chunk) -> void {
		// Register our tile entities
		_world.RegisterChunkTileEntities(_chunk.get());

		// Register our entities
		for (auto& entityTag : _chunk->entityTags) {
			_world.entityManager.CreateEntityFromNbt(entityTag);
		}
		_chunk->entityTags.clear();
		_chunk->entityTags.shrink_to_fit();

		_world.chunks[_chunk->cpos] = std::move(_chunk);
	};

	auto loadAlphaChunk = [&](FileHandle& _cFile) -> std::shared_ptr<Chunk> {
		auto& chunkFile = _cFile.Get();

		// Get the length of the file
		chunkFile.seekg(0, std::ios::end);
		std::streamsize size = chunkFile.tellg();
		chunkFile.seekg(0, std::ios::beg);

		std::vector<char> buffer(size);
		chunkFile.read(buffer.data(), size);
		char* compressedChunk = buffer.data();

		size_t compressedSize = size;
		size_t decompressedSize = 81920 * 2;

		auto chunkData = decompressChunk(compressedChunk, compressedSize, decompressedSize);
		if (!chunkData) {
			Logger::Warning << "Failed to decompress alpha chunk, skipping.\n";
			return nullptr;
		}
		uint8_t* pdata = reinterpret_cast<uint8_t*>(chunkData.get());
		std::vector<uint8_t> nbtData(pdata, pdata + decompressedSize);

		Region region;
		auto chunkFromNBT = region.DecodeDecompressedNbtData(nbtData);
		if (!chunkFromNBT) {
			Logger::Warning << "Failed to decode NBT for alpha chunk, skipping.\n";
			return nullptr;
		}
		chunkFromNBT->isModified = true;
		return chunkFromNBT;
	};

	if (!fs::exists(_dir) || !fs::is_directory(_dir))
		return false;

	std::vector<fs::path> result;
	std::vector<fs::path> netherResult;
	std::error_code ec;

	// skip_permission_denied keeps a single unreadable folder from aborting
	auto it = fs::recursive_directory_iterator(_dir, fs::directory_options::skip_permission_denied, ec);
	auto end = fs::recursive_directory_iterator();

	// Collect chunk paths
	for (; it != end; it.increment(ec)) {
		if (ec) {
			// A single entry failed
			ec.clear();
			continue;
		}

		const fs::directory_entry& entry = *it;
		if (!entry.is_regular_file(ec))
			continue;

		std::string filename = entry.path().filename().string();
		if (isChunkFilename(filename)) {
			if (isInNetherSubtree(_dir, entry.path())) {
				netherResult.push_back(entry.path());
			} else {
				result.push_back(entry.path());
			}
		}
	}

	// Create a temp path
	std::string tempDir = _dir + "_temp";
	recreateTempDir(tempDir);
	recreateTempDir(tempDir + "/region");
	recreateTempDir(tempDir + "/DIM-1/region");

	// Carry over anything the conversion itself doesn't touch
	for (const char* filename : { "level.dat", "level.dat_old" }) {
		fs::path src = fs::path(_dir) / filename;
		if (fs::exists(src)) {
			fs::copy(src, fs::path(tempDir) / filename, fs::copy_options::overwrite_existing);
		}
	}

	fs::path srcPlayers = fs::path(_dir) / "players";
	if (fs::exists(srcPlayers)) {
		fs::copy(srcPlayers, fs::path(tempDir) / "players",
		         fs::copy_options::recursive | fs::copy_options::overwrite_existing);
	}

	// Setup our region managers
	RegionManager overworldRegionManager;
	RegionManager hellRegionManager;
	overworldRegionManager.Initialize(tempDir + "/region");
	hellRegionManager.Initialize(tempDir + "/DIM-1/region");

	// Setup our world(s)
	WorldManager world;
	WorldManager hellWorld(true);

	world.regionManager = &overworldRegionManager;
	hellWorld.regionManager = &hellRegionManager;
	overworldRegionManager.world = &world;
	hellRegionManager.world = &hellWorld;
	constexpr size_t FLUSH_BATCH_SIZE = 200;

	// Convert overworld
	Logger::Info << "Converting overworld...\n";
	size_t sinceFlush = 0;
	size_t chunksProcessed = 0;
	for (auto& chunkPath : result) {
		FileHandle chunkFileHandle(chunkPath);
		auto chunk = loadAlphaChunk(chunkFileHandle);
		if (chunk) {
			// Alpha lighting tends to be buggy..
			chunk->refreshLighting = true;

			registerChunk(world, chunk);

			if (++sinceFlush >= FLUSH_BATCH_SIZE) {
				world.SaveChunks(/*saveIfEntities=*/true);
				overworldRegionManager.FlushAll();
				sinceFlush = 0;
				world.chunks.clear();
				world.InvalidateChunkCache();
			}
		}
		chunksProcessed++;

		if (chunksProcessed % 100 == 0 || chunksProcessed >= result.size()) {
			Logger::Info << "Processed " << chunksProcessed << "/" << result.size() << "\n";
		}
	}
	world.DrainLoadQueue();
	world.SaveChunks(/*saveIfEntities=*/true);

	// Convert nether
	Logger::Info << "Converting nether...\n";
	sinceFlush = 0;
	chunksProcessed = 0;
	for (auto& chunkPath : netherResult) {
		FileHandle chunkFileHandle(chunkPath);
		auto chunk = loadAlphaChunk(chunkFileHandle);
		if (chunk) {
			// Alpha lighting tends to be buggy..
			chunk->refreshLighting = true;

			registerChunk(hellWorld, chunk);

			if (++sinceFlush >= FLUSH_BATCH_SIZE) {
				hellWorld.SaveChunks(/*saveIfEntities=*/true);
				hellRegionManager.FlushAll();
				sinceFlush = 0;
				hellWorld.chunks.clear();
				hellWorld.InvalidateChunkCache();
			}
		}
		chunksProcessed++;

		if (chunksProcessed % 100 == 0 || chunksProcessed >= netherResult.size()) {
			Logger::Info << "Processed " << chunksProcessed << "/" << netherResult.size() << "\n";
		}
	}
	hellWorld.DrainLoadQueue();
	hellWorld.SaveChunks(/*saveIfEntities=*/true);

	// Save everything
	overworldRegionManager.FlushAll();
	hellRegionManager.FlushAll();

	// Shutdown the worlds
	world.Shutdown();
	hellWorld.Shutdown();

	// Release the region managers
	overworldRegionManager.Release();
	hellRegionManager.Release();

	world.chunks.clear();
	world.InvalidateChunkCache();
	hellWorld.chunks.clear();
	hellWorld.InvalidateChunkCache();

	// Move the original world
	std::string oldDir = _dir + "_old";

	// Clear out any leftover world_old
	ec.clear();
	fs::remove_all(oldDir, ec);
	if (ec) {
		Logger::Error << "Failed to clear old backup directory '" << oldDir << "': " << ec.message() << "\n";
		return false;
	}

	ec.clear();
	fs::rename(_dir, oldDir, ec);
	if (ec) {
		Logger::Error << "Failed to rename original world to '" << oldDir << "': " << ec.message() << "\n";
		return false;
	}

	ec.clear();
	fs::rename(tempDir, _dir, ec);
	if (ec) {
		Logger::Error << "Failed to rename converted world into place: " << ec.message() << "\n";
		// Try to restore the original so we don't leave the world directory missing entirely.
		std::error_code restoreEc;
		fs::rename(oldDir, _dir, restoreEc);
		if (restoreEc) {
			Logger::Error << "Failed to restore original world after failed conversion! Your original "
			                        "world is at '"
			                     << oldDir << "'.\n";
		}
		return false;
	}

	return true;
}

// Converts an old betrock server world to mcregion
bool convertBetrockServerLevel(std::string& _dir) {
	Blocks::RegisterAll();
	RegionManager regionManager;
	SaveManager saveManager;
	WorldManager world;
	recreateTempDir("ConvertedWorld");

	saveManager.Initialize("ConvertedWorld", true);
	saveManager.CreateNewWorld(/*level data=*/{});

	fs::path srcPlayers = fs::path(_dir) / "players";
	fs::path srcRegion = fs::path(_dir) / "region";

	world.regionManager = &regionManager;

	if (!fs::exists(srcRegion)) {
		Logger::Error << "Invalid Betrock level file!\n";
		return false;
	}

	if (!regionManager.Initialize("ConvertedWorld/region")) {
		Logger::Error << "Failed to initialize overworld region manager for conversion!\n";
		return false;
	}

	if (fs::exists(srcPlayers)) {
		fs::copy(srcPlayers, "ConvertedWorld/players",
		         fs::copy_options::recursive | fs::copy_options::overwrite_existing);
	} else {
		Logger::Warning << "No Betrock player files found!\n";
	}

	auto blockIndexToPosition = [](int32_t _index) -> Int3 {
		Int3 pos;
		pos.y = _index % CHUNK_HEIGHT;
		_index /= CHUNK_HEIGHT;

		pos.z = _index % CHUNK_WIDTH;
		_index /= CHUNK_WIDTH;

		pos.x = _index;
		return pos;
	};

	auto decompressChunk = [](const char* _compressedData, size_t _compressedSize, size_t& _decompressedSize) -> std::unique_ptr<char[]> {
		// TODO: Where does 81920 come from??? Who wrote this? Keeping it just because it seems to not have fucked up thus far...
		auto r = Deflate::Decompress(
			Deflate::Format::Gzip,
			{ reinterpret_cast<const uint8_t*>(_compressedData), _compressedSize },
			_decompressedSize ? _decompressedSize : 81920
		);
		if (!r) return nullptr;
		auto out = std::make_unique<char[]>(r->size());
		std::memcpy(out.get(), r->data(), r->size());
		_decompressedSize = r->size();
		return out;
	};

	auto loadOldFormat = [&](FileHandle& _cFile) -> std::shared_ptr<Chunk> {
		auto& chunkFile = _cFile.Get();

		// Get the length of the file
		chunkFile.seekg(0, std::ios::end);
		std::streamsize size = chunkFile.tellg();
		chunkFile.seekg(0, std::ios::beg);

		std::vector<char> buffer(size);
		chunkFile.read(buffer.data(), size);
		char* compressedChunk = buffer.data();

		size_t compressedSize = size;
		size_t decompressedSize = 0;

		auto chunkData = decompressChunk(compressedChunk, compressedSize, decompressedSize);

		if (!chunkData)
			return nullptr;

		std::shared_ptr<Chunk> c = std::make_shared<Chunk>();
		size_t blockDataSize = CHUNK_WIDTH * CHUNK_WIDTH * CHUNK_HEIGHT;
		size_t nibbleDataSize = CHUNK_WIDTH * CHUNK_WIDTH * (CHUNK_HEIGHT / 2);
		for (size_t i = 0; i < decompressedSize; i++) {
			if (i < blockDataSize) {
				// Block Data
				c->SetBlock(blockIndexToPosition(i), BlockType(chunkData[i]));
			} else if (
			    // Metadata
			    i >= blockDataSize && i < blockDataSize + nibbleDataSize) {
				c->SetMeta(blockIndexToPosition((i % nibbleDataSize) * 2), chunkData[i] & 0xF);
				c->SetMeta(blockIndexToPosition((i % nibbleDataSize) * 2 + 1), (chunkData[i] >> 4) & 0xF);
			} else if (
			    // Block Light
			    i >= blockDataSize + nibbleDataSize && i < blockDataSize + (nibbleDataSize * 2)) {
				c->SetBlockLight(blockIndexToPosition((i % nibbleDataSize) * 2), chunkData[i] & 0xF);
				c->SetBlockLight(blockIndexToPosition((i % nibbleDataSize) * 2 + 1), (chunkData[i] >> 4) & 0xF);
			} else if (
			    // Sky Light
			    i >= blockDataSize + (nibbleDataSize * 2) && i < blockDataSize + (nibbleDataSize * 3)) {
				c->SetSkyLight(blockIndexToPosition((i % nibbleDataSize) * 2), chunkData[i] & 0xF);
				c->SetSkyLight(blockIndexToPosition((i % nibbleDataSize) * 2 + 1), (chunkData[i] >> 4) & 0xF);
			}
		}
		c->isTerrainPopulated = true;
		c->isModified = true;
		c->state = ChunkState::Populated;

		return c;
	};

	auto loadV2Format = [&](FileHandle& _cFile) -> std::shared_ptr<Chunk> {
		auto& chunkFile = _cFile.Get();

		// Get the length of the file
		chunkFile.seekg(0, std::ios::end);
		std::streamsize size = chunkFile.tellg();
		chunkFile.seekg(0, std::ios::beg);

		std::vector<char> buffer(size);
		chunkFile.read(buffer.data(), size);
		char* compressedChunk = buffer.data();

		size_t compressedSize = size;
		size_t decompressedSize = 81920 * 2;

		auto chunkData = decompressChunk(compressedChunk, compressedSize, decompressedSize);
		uint8_t* pdata = reinterpret_cast<uint8_t*>(chunkData.get());

		auto nbt = NBTParser(pdata, decompressedSize);

		const size_t blockDataSize = (CHUNK_WIDTH * CHUNK_WIDTH * CHUNK_HEIGHT);
		const size_t nibbleDataSize = (CHUNK_WIDTH * CHUNK_WIDTH * (CHUNK_HEIGHT / 2));

		std::shared_ptr<Chunk> c = std::make_shared<Chunk>();

		auto& root = nbt.root;
		auto& level = root.Get("Level");
		auto& blocksTag = level.Get("Blocks");
		auto& metaTag = level.Get("Data");
		auto& metaData = metaTag.byteArray;

		// Block ids
		for (size_t i = 0; i < blockDataSize; i++) {
			c->SetBlock(blockIndexToPosition(i), BlockType(blocksTag.byteArray[i]));
		}

		// Metadata
		for (size_t i = 0; i < nibbleDataSize; i++) {
			c->SetMeta(blockIndexToPosition(i * 2), (metaData[i]) & 0xF);
			c->SetMeta(blockIndexToPosition(i * 2 + 1), (metaData[i] >> 4) & 0xF);
		}

		c->isTerrainPopulated = true;
		c->isModified = true;
		c->state = ChunkState::Populated;

		return c;
	};

	std::vector<Int32_2> chunkCoords;
	for (const auto& entry : fs::directory_iterator(srcRegion)) {
		const fs::path& regionPath = entry.path();
		// Do the old format chunks first
		if (regionPath.extension() == ".cnk") {
			const std::string filename = entry.path().filename().string();
			int rx, rz;

			// Does this have a valid name?
			if (std::sscanf(filename.c_str(), "%d,%d.cnk", &rx, &rz) == 2) {
				chunkCoords.push_back({ rx, rz });
			}
		}
	}

	Logger::Info << "Found " << chunkCoords.size() << " OLD format chunks to convert.\n";

	// Save all our old format chunks
	int count = 0;
	for (auto& cpos : chunkCoords) {
		auto path = srcRegion / (std::to_string(cpos.x) + "," + std::to_string(cpos.z) + ".cnk");
		FileHandle fileHandle(path);
		auto chunk = loadOldFormat(fileHandle);
		if (chunk) {
			count++;
			chunk->cpos = { cpos.x, cpos.z };
			chunk->refreshLighting = true;
			world.chunks[chunk->cpos] = chunk;

			if (count % 256 == 0) {
				world.SaveChunks(true);
				regionManager.FlushAll();
				world.chunks.clear();
				world.InvalidateChunkCache();
			}
		}
	}

	Logger::Info << "Converted! Now converting V2 format chunks..\n";

	// V2 chunks
	chunkCoords.clear();
	for (const auto& entry : fs::directory_iterator(srcRegion)) {
		const fs::path& regionPath = entry.path();
		// Do the old format chunks first
		if (regionPath.extension() == ".ncnk") {
			const std::string filename = entry.path().filename().string();
			int rx, rz;

			// Does this have a valid name?
			if (std::sscanf(filename.c_str(), "%d,%d.ncnk", &rx, &rz) == 2) {
				chunkCoords.push_back({ rx, rz });
			}
		}
	}

	Logger::Info << "Found " << chunkCoords.size() << " V2 format chunks to convert.\n";

	// Save all our V2 format chunks
	for (auto& cpos : chunkCoords) {
		auto path = srcRegion / (std::to_string(cpos.x) + "," + std::to_string(cpos.z) + ".ncnk");
		FileHandle fileHandle(path);
		auto chunk = loadV2Format(fileHandle);
		if (chunk) {
			count++;
			chunk->cpos = { cpos.x, cpos.z };
			chunk->refreshLighting = true;
			world.chunks[chunk->cpos] = chunk;

			if (count % 256 == 0) {
				world.SaveChunks(true);
				regionManager.FlushAll();
				world.chunks.clear();
				world.InvalidateChunkCache();
			}
		}
	}
	Logger::Info << "Finishing up..\n";

	// Save
	world.Shutdown();

	Logger::Info << "Done!\n";

	return true;
}
}; // namespace Utilities