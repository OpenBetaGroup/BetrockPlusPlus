/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once
#include "BS_thread_pool.hpp"
#include "region.h"
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct SnapshotContainer {
	std::shared_ptr<Chunk> chunkSnapshot;
	std::shared_ptr<const std::vector<Tag>> entitySnapshot;
};

class WorldManager;

struct RegionManager {
	BS::thread_pool<> iopool{ 2 };

	static constexpr size_t MAX_PENDING_REGIONS = 32;

	std::mutex saveQueueMutex;
	std::vector<SnapshotContainer> saveQueue;

	std::mutex outChunksMutex;
	std::unordered_map<Int32_2, std::shared_ptr<Chunk>> outChunks;

	// Positions whose async/sync load could not produce a chunk (retry as Unloaded).
	std::mutex failedLoadsMutex;
	std::unordered_set<Int32_2> failedLoads;

	// As much as I hate to do this it makes my job easier
	WorldManager* world = nullptr;

	~RegionManager();

	bool Initialize(const std::string& _folderPath);
	bool Release();

	// Does this region file exist on the disk?
	bool RegionExistsOnDisk(const Int32_2 _rpos);

	// Does this region exist in memory or on the disk?
	bool RegionExists(const Int32_2 _rpos);

	// Has this chunk been saved to a region file yet?
	bool ChunkExists(const Int32_2 _cpos);

	// Creates a new region file
	bool CreateRegion(const Int32_2 _rpos);

	// Serialize and save a chunk to a region
	void SaveChunk(const std::shared_ptr<Chunk> _chunk, bool _unloadEntities = false);

	// Queue a chunk to be loaded from disk
	void LoadChunk(const Int32_2 _cpos);

	// Returns nullptr until chunk is done loading
	std::shared_ptr<Chunk> GetChunk(const Int32_2 _cpos);

	// Drop a pending/finished load result (e.g. chunk left the load radius).
	void DiscardChunk(const Int32_2 _cpos);

	// Pop and clear any load failures since the last call.
	std::vector<Int32_2> TakeFailedLoads();

	void PumpPipeline();

	// Flush all pending saves synchronously
	void FlushAll();

	// Loads a region into cache, creating the file if needed
	enum LoadState : uint8_t {
		FAILED,
		CACHE_BUSY,
		SUCCESS
	};
	std::shared_ptr<Region> LoadRegion(const Int32_2 _rpos, LoadState& _state);

	void FatalIo(const std::string& _what, Int32_2 _cpos);
	bool HasFatalIo() const {
		return ioFatal.load();
	}

private:
	bool TryMergePendingRegion(std::shared_ptr<Region>& _region);
	bool CreateRegionOnCache(const Int2 _rpos);
	void MarkLoadFailed(const Int32_2 _cpos);

	std::atomic<bool> ioFatal{ false };

	std::vector<std::shared_ptr<Region>> pendingRegions;
	std::shared_ptr<Region> regionCache[8];
	int cacheIndex = 0;
	std::string mFolderPath; // Path to where all the regions get dumped
};