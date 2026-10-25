/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once
#include "chunk.h"
#include "helpers/endian_convert.h"
#include "helpers/file_handle.h"
#include "numeric_structs.h"
#include <memory>
#include <mutex>

#define REGION_WIDTH 32
#define REGION_AREA REGION_WIDTH* REGION_WIDTH
#define SECTOR_SIZE 4096

inline std::string RegionPositionToFileName(Int2 _rpos) {
	return "r." + std::to_string(_rpos.x) + "." + std::to_string(_rpos.z) + ".mcr";
}

enum CompressorFormat : uint8_t {
	REGION_INVALID = 0,
	REGION_GZIP = 1,
	REGION_ZLIB = 2
};

struct FileHeaderEntry {
	uint32_t offset : 24;
	uint32_t numberOfSectors : 8;
	// TODO: Maybe store last-updated here?
};

class Region {
public:
	Int32_2 rpos;
	std::mutex mutex;
	Region() {};
	Region(Int32_2 _rpos, std::string _folderPath)
	    : rpos(_rpos), regionFile(_folderPath + "/" + RegionPositionToFileName(_rpos)) {
		// Cache our header
		ReadHeaderFromFile();
	}
	bool ChunkExists(Int2 _localcpos) {
		int index = _localcpos.x + _localcpos.z * 32;
		auto* rHeader = &regionHeader[index];
		return (rHeader->numberOfSectors != 0 && rHeader->offset != 0);
	}

	void AddChunk(std::shared_ptr<Chunk> _chunk, int64_t _timestamp, std::shared_ptr<const std::vector<Tag>> _entities);
	std::shared_ptr<Chunk> GetChunk(Int32_2 _cpos);

	// Read our header data into the "regionHeader"
	void ReadHeaderFromFile() {
		auto& file = regionFile.Get();
		file.seekg(0); // Beginning of sector 0
		for (int i = 0; i < 1024; i++) {
			uint32_t entry;
			file.read(reinterpret_cast<char*>(&entry), 4);
			entry = FromBigEndian(entry);
			regionHeader[i].numberOfSectors = entry & 0xFF; // bottom 1 byte
			regionHeader[i].offset = entry >> 8;            // top 3 bytes
		}
	}

	std::vector<uint8_t> EncodeNbtData(const std::shared_ptr<Chunk>& _chunk, int64_t _timestamp,
	                                   std::shared_ptr<const std::vector<Tag>> _entities);
	std::shared_ptr<Chunk> DecodeNbtData(const std::vector<uint8_t>& _rawData);
	std::shared_ptr<Chunk> DecodeDecompressedNbtData(std::vector<uint8_t>& _decompressedData);

private:
	//std::array<std::shared_ptr<Chunk>, REGION_AREA> chunks;
	std::string GetPath();
	std::array<FileHeaderEntry, 1024> regionHeader;
	FileHandle regionFile;
};