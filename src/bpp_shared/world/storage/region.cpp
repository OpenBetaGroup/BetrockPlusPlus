/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "region.h"
#include "chunk.h"
#include "logger.h"
#include "nbt/nbt.h"
#include "platform/memory.h"
#include "tile_entities/tile_entity.h"
#include <algorithm>
#include <cstdint>
#include "helpers/endian_convert.h"
#include <fstream>
#include "helpers/deflate_pool.h"
#include <memory>
#include <string>

namespace {
constexpr size_t CHUNK_DATA_SIZE = (16 * 128 * 16) * 5 / 2;
constexpr size_t HEIGHT_MAP_DATA_SIZE = 16 * 16;
// * 1.1 for a bit of headroom for entities, tile entities, storage contents, etc.
// TODO: Calculate a more accurate baseline that takes into account
// - The NBT Tag bytes themselves
// - Any always-existing NBT labels
constexpr size_t BASE_MEMORY_ALLOCATION_GUESS = CHUNK_DATA_SIZE + HEIGHT_MAP_DATA_SIZE * 1.1;

// First guess for how big a chunk's decompressed NBT is,
// rounded up to a whole number of memory pages
size_t InitialDecompressedSize() {
	static const size_t size = []() -> size_t {
		const size_t page = std::max<size_t>(GetPageSize(), 1);
		return ((BASE_MEMORY_ALLOCATION_GUESS + page - 1) / page) * page;
	}();
	return size;
}
}

void Region::AddChunk(std::shared_ptr<Chunk> _chunk, int64_t _timestamp,
                      std::shared_ptr<const std::vector<Tag>> _entities) {
	auto compressed = EncodeNbtData(_chunk, _timestamp, _entities);
	if (compressed.empty())
		return;

	// Only lock after encoding (hehe)
	std::lock_guard<std::mutex> lock(mutex);

	// Chunk header: 4 bytes length + 1 byte compression type
	uint32_t payloadLength = uint32_t(compressed.size()) + 1; // +1 for the format byte
	uint32_t sectorsNeeded = (payloadLength + 4 + SECTOR_SIZE - 1) / SECTOR_SIZE;

	// Find a free run of sectors in the file
	// Build an occupancy set from the header
	std::vector<bool> occupied;
	for (int i = 0; i < 1024; i++) {
		if (regionHeader[i].offset == 0)
			continue;
		uint32_t end = regionHeader[i].offset + regionHeader[i].numberOfSectors;
		if (end > occupied.size())
			occupied.resize(end, false);
		for (uint32_t s = regionHeader[i].offset; s < end; s++)
			occupied[s] = true;
	}

	// Find first free run of `sectorsNeeded` sectors starting at 2 (after header)
	uint32_t chosenOffset = 0;
	for (uint32_t s = 2;; s++) {
		bool fits = true;
		for (uint32_t j = 0; j < sectorsNeeded; j++) {
			if (s + j < occupied.size() && occupied[s + j]) {
				fits = false;
				break;
			}
		}
		if (fits) {
			chosenOffset = s;
			break;
		}
	}

	// Write chunk data at the chosen sector
	auto& file = regionFile.Get();
	file.seekp(chosenOffset * SECTOR_SIZE);

	// 4-byte big-endian length
	uint32_t lengthBE = ToBigEndian(payloadLength);
	file.write(reinterpret_cast<char*>(&lengthBE), 4);

	// 1-byte compression type
	CompressorFormat format = CompressorFormat::REGION_ZLIB;
	file.write(reinterpret_cast<char*>(&format), 1);

	// Compressed data
	file.write(reinterpret_cast<char*>(compressed.data()), compressed.size());

	// Pad to sector boundary
	size_t written = 5 + compressed.size();
	size_t padded = sectorsNeeded * SECTOR_SIZE;
	if (written < padded) {
		std::vector<char> pad(padded - written, 0);
		file.write(pad.data(), pad.size());
	}

	// Update header entry
	Int2 local{ _chunk->cpos.x & 31, _chunk->cpos.z & 31 };
	int index = local.x + local.z * 32;
	regionHeader[index].offset = chosenOffset;
	regionHeader[index].numberOfSectors = uint8_t(sectorsNeeded);

	// Write updated header entry to file
	file.seekp(index * 4);
	uint32_t entry = (chosenOffset << 8) | uint8_t(sectorsNeeded);
	entry = ToBigEndian(entry);
	file.write(reinterpret_cast<char*>(&entry), 4);
	file.flush();
}

std::shared_ptr<Chunk> Region::GetChunk(Int32_2 _cpos) {
	std::lock_guard<std::mutex> lock(mutex);
	Int2 local{ _cpos.x & 31, _cpos.z & 31 };
	int index = local.x + local.z * 32;

	if (!ChunkExists(local))
		return nullptr;

	uint32_t offset = regionHeader[index].offset;
	if (offset == 0)
		return nullptr;

	auto& file = regionFile.Get();
	file.seekg(static_cast<std::streamoff>(offset) * SECTOR_SIZE);

	// Read 4-byte length
	uint32_t length = 0;
	file.read(reinterpret_cast<char*>(&length), 4);
	if (file.fail())
		return nullptr;
	length = FromBigEndian(length);

	// Guard against corrupt/zero length before subtracting
	if (length < 1) {
		Logger::Warning << "Invalid chunk length: " << length << "\n";
		return nullptr;
	}

	// Read compression type
	CompressorFormat format = CompressorFormat::REGION_INVALID;
	file.read(reinterpret_cast<char*>(&format), 1);
	if (file.fail())
		return nullptr;

	if (format != CompressorFormat::REGION_ZLIB) {
		Logger::Warning << "Unsupported compression format: " << int(format) << "\n";
		return nullptr;
	}

	// Read compressed data (length includes the format byte, so actual data is length-1)
	std::vector<uint8_t> compressed(length - 1);
	file.read(reinterpret_cast<char*>(compressed.data()), length - 1);
	if (file.fail())
		return nullptr;

	return DecodeNbtData(compressed);
}

std::vector<uint8_t> Region::EncodeNbtData(const std::shared_ptr<Chunk>& _chunk, int64_t _timestamp,
                                           std::shared_ptr<const std::vector<Tag>> _entities) {
	// Build a compound tag representing a chunk level entry
	Tag root;
	root.type = TAG_COMPOUND;
	root.name = "";

	Tag level;
	level.type = TAG_COMPOUND;
	level.name = "Level";

	// Scalar values
	Tag xPos;
	xPos.type = TAG_INT;
	xPos.name = "xPos";
	xPos.intValue = _chunk->cpos.x;
	Tag zPos;
	zPos.type = TAG_INT;
	zPos.name = "zPos";
	zPos.intValue = _chunk->cpos.z;
	Tag populated;
	populated.type = TAG_BYTE;
	populated.name = "TerrainPopulated";
	populated.byteValue = _chunk->isTerrainPopulated;
	Tag lastUpdate;
	lastUpdate.type = TAG_LONG;
	lastUpdate.name = "LastUpdate";
	lastUpdate.longValue = _timestamp;

	// Byte array, blocks
	Tag blocksTag;
	blocksTag.type = TAG_BYTEARRAY;
	blocksTag.name = "Blocks";
	blocksTag.byteArray.resize((CHUNK_WIDTH * CHUNK_WIDTH * CHUNK_HEIGHT), 0);

	// Nibble arrays (4 bits per block, so half the size of blocks array)
	Tag dataTag;
	dataTag.type = TAG_BYTEARRAY;
	dataTag.name = "Data";
	dataTag.byteArray.resize((CHUNK_WIDTH * CHUNK_WIDTH * CHUNK_HEIGHT) / 2, 0);

	Tag blockLightTag;
	blockLightTag.type = TAG_BYTEARRAY;
	blockLightTag.name = "BlockLight";
	blockLightTag.byteArray.resize((CHUNK_WIDTH * CHUNK_WIDTH * CHUNK_HEIGHT) / 2, 0);

	Tag skyLightTag;
	skyLightTag.type = TAG_BYTEARRAY;
	skyLightTag.name = "SkyLight";
	skyLightTag.byteArray.resize((CHUNK_WIDTH * CHUNK_WIDTH * CHUNK_HEIGHT) / 2, 0);

	// HeightMap, one byte per (x,z) column
	Tag heightMap;
	heightMap.type = TAG_BYTEARRAY;
	heightMap.name = "HeightMap";
	heightMap.byteArray.resize(CHUNK_WIDTH * CHUNK_WIDTH, 0);
	std::copy(std::begin(_chunk->heightMap), std::end(_chunk->heightMap), heightMap.byteArray.data());

	// Put blocks in there
	{
		int8_t* const blocksOut = blocksTag.byteArray.data();
		int8_t* const dataOut = dataTag.byteArray.data();
		int8_t* const skyLightOut = skyLightTag.byteArray.data();
		int8_t* const blockLightOut = blockLightTag.byteArray.data();
		for (int x = 0; x < CHUNK_WIDTH; x++) {
			for (int z = 0; z < CHUNK_WIDTH; z++) {
				for (int s = 0; s < SUB_CHUNK_COUNT; s++) {
					const SubChunk& sub = _chunk->subChunks[size_t(s)];
					const SubChunk::BlockLayer* bl = sub.blocks.get();
					const SubChunk::NibbleLayer* ml = sub.meta.get();
					const SubChunk::NibbleLayer* bll = sub.blockLight.get();
					const SubChunk::NibbleLayer* sl = sub.skyLight.get();
					const int8_t blockFill = int8_t(sub.blockFill);
					const uint8_t mf = sub.metaFill, blf = sub.blockLightFill, sf = sub.skyLightFill;
					for (int ly = 0; ly < SUB_CHUNK_SIZE; ly++) {
						const int y = s * SUB_CHUNK_SIZE + ly;
						const size_t idx = size_t(y + (z * CHUNK_HEIGHT) + (x * CHUNK_HEIGHT * CHUNK_WIDTH));
						const int li = SubChunk::LocalIndex({ x, y, z });
						blocksOut[idx] = bl ? int8_t(bl->types[size_t(li)]) : blockFill;
						const uint8_t meta = ml ? ml->Get(li) : mf;
						const uint8_t skyLight = sl ? sl->Get(li) : sf;
						const uint8_t blockLight = bll ? bll->Get(li) : blf;
						const uint8_t shift = (y & 1) ? 4u : 0u;
						dataOut[idx / 2] = int8_t(dataOut[idx / 2] | (meta << shift));
						skyLightOut[idx / 2] = int8_t(skyLightOut[idx / 2] | (skyLight << shift));
						blockLightOut[idx / 2] = int8_t(blockLightOut[idx / 2] | (blockLight << shift));
					}
				}
			}
		}
	}

	// List tag for entities
	Tag entitiesTag;
	entitiesTag.type = TAG_LIST;
	entitiesTag.name = "Entities";
	entitiesTag.listType = TAG_COMPOUND;
	if (_entities) {
		entitiesTag.list.insert(entitiesTag.list.end(), _entities->begin(), _entities->end());
	}

	// Nested compound inside a list for tile entities
	Tag tileEntities;
	tileEntities.type = TAG_LIST;
	tileEntities.name = "TileEntities";
	tileEntities.listType = TAG_COMPOUND;
	for (auto& te : _chunk->tileEntities) {
		if (te)
			tileEntities.list.push_back(te->Serialize());
	}

	// Assemble level compound
	level.compound["xPos"] = xPos;
	level.compound["zPos"] = zPos;
	level.compound["TerrainPopulated"] = populated;
	level.compound["LastUpdate"] = lastUpdate;
	level.compound["Blocks"] = std::move(blocksTag);
	level.compound["Data"] = std::move(dataTag);
	level.compound["BlockLight"] = std::move(blockLightTag);
	level.compound["SkyLight"] = std::move(skyLightTag);
	level.compound["HeightMap"] = std::move(heightMap);
	level.compound["Entities"] = std::move(entitiesTag);
	level.compound["TileEntities"] = std::move(tileEntities);

	// Only include this tag if we need to reseed lighting
	if (_chunk->refreshLighting) {
		Tag refreshLighting;
		refreshLighting.type = TAG_BYTE;
		refreshLighting.name = "RefreshLighting";
		refreshLighting.byteValue = _chunk->refreshLighting ? 1 : 0;

		level.compound["RefreshLighting"] = refreshLighting;
	}

	root.compound["Level"] = std::move(level);

	// Serialize to bytes
	std::vector<uint8_t> raw;
	NBTwriter writer(raw, root);
	// Compress
	return Deflate::Compress(Deflate::Format::Zlib, raw, 1);
}

std::shared_ptr<Chunk> Region::DecodeNbtData(const std::vector<uint8_t>& _rawData) {
	auto decompressed = Deflate::Decompress(Deflate::Format::Zlib, _rawData, InitialDecompressedSize());
	if (!decompressed) {
		Logger::Warning << "Decompression failed!\n";
		return std::make_shared<Chunk>();
	}
	return DecodeDecompressedNbtData(*decompressed);
}

std::shared_ptr<Chunk> Region::DecodeDecompressedNbtData(std::vector<uint8_t>& _decompressedData) {
	NBTParser parser(_decompressedData.data(), int64_t(_decompressedData.size()));
	const Tag& lvl = parser.root.Get("Level");

	int32_t cx = lvl.Get("xPos").GetInt();
	int32_t cz = lvl.Get("zPos").GetInt();
	bool tp = true;
	lvl.Has("TerrainPopulated") ? tp = lvl.Get("TerrainPopulated").GetByte() != 0 : tp = 1;
	lvl.Get("LastUpdate").GetLong();

	const auto& blocks = lvl.Get("Blocks").GetByteArray();
	const auto& data = lvl.Get("Data").GetByteArray();
	const auto& blockLight = lvl.Get("BlockLight").GetByteArray();
	const auto& skyLight = lvl.Get("SkyLight").GetByteArray();
	const auto& heightMap = lvl.Get("HeightMap").GetByteArray();
	const auto& tileEntities = lvl.Get("TileEntities").GetList();

	std::vector<Tag> empty = {};
	const auto& entities = lvl.Has("Entities") ? lvl.Get("Entities").GetList() : empty;

	// Setup our chunk
	auto chunk = std::make_shared<Chunk>();
	chunk->cpos = Int32_2{ cx, cz };
	chunk->state = tp ? ChunkState::Populated : ChunkState::Generated;
	chunk->isTerrainPopulated = tp;
	chunk->refreshLighting = lvl.Has("RefreshLighting") ? lvl.Get("RefreshLighting").byteValue : false;
	std::copy(heightMap.begin(), heightMap.end(), chunk->heightMap);

	// Load all of our block data
	const int8_t* const blocksIn = blocks.data();
	const int8_t* const dataIn = data.data();
	const int8_t* const blightIn = blockLight.data();
	const int8_t* const skyIn = skyLight.data();
	bool any = false;
	for (int y = 0; y < CHUNK_HEIGHT; y++) {
		SubChunk& sub = chunk->subChunks[size_t(y >> 4)];
		const unsigned shift = (y & 1) ? 4u : 0u;
		for (int x = 0; x < CHUNK_WIDTH; x++) {
			for (int z = 0; z < CHUNK_WIDTH; z++) {
				size_t idx = size_t(y + (z * CHUNK_HEIGHT) + (x * CHUNK_HEIGHT * CHUNK_WIDTH));
				// Make sure this block is valid
				BlockType thisBlock = BlockType(blocksIn[idx]);
				if (!Items::IsBlock(thisBlock))
					continue;
				any = true;
				const int li = SubChunk::LocalIndex({ x, y, z });
				sub.SetBlock(li, thisBlock);
				sub.SetMeta(li, uint8_t((uint8_t(dataIn[idx / 2]) >> shift) & 0xF));
				sub.SetBlockLight(li, uint8_t((uint8_t(blightIn[idx / 2]) >> shift) & 0xF));
				sub.SetSkyLight(li, uint8_t((uint8_t(skyIn[idx / 2]) >> shift) & 0xF));
			}
		}
		if ((y & (SUB_CHUNK_SIZE - 1)) == SUB_CHUNK_SIZE - 1)
			chunk->TryToCompactSubChunk(y / SUB_CHUNK_SIZE);
	}
	if (any)
		chunk->isModified = true;

	// Load our entities
	chunk->entityTags = std::move(entities);

	// Load our tile entities
	for (auto& te : tileEntities) {
		const auto& id = te.Get("id").GetString();
		int32_t tx = te.Get("x").GetInt();
		int32_t ty = te.Get("y").GetInt();
		int32_t tz = te.Get("z").GetInt();
		Int3 pos{ tx, ty, tz };

		// load a standard slot-based inventory from an Items list tag
		auto loadSlots = [&](auto& _slots) {
			if (!te.Has("Items"))
				return;
			for (auto& item : te.Get("Items").GetList()) {
				int8_t slot = item.Get("Slot").GetByte();
				int16_t itemId = item.Get("id").GetShort();
				int8_t count = item.Get("Count").GetByte();
				int16_t damage = item.Get("Damage").GetShort();
				if (slot >= 0 && slot < int8_t(_slots.size())) {
					_slots[size_t(slot)] = ItemStack{ itemId, count, damage };
				}
			}
		};

		//TODO: Add de-serialize method to TileEntity instead of this.. thing
		if (id == "Chest") {
			auto ent = std::make_shared<TileEntityChest>(pos);
			loadSlots(ent->inventory.slots);
			chunk->tileEntities.push_back(std::move(ent));
		} else if (id == "Furnace") {
			int burnTime = te.Has("BurnTime") ? te.Get("BurnTime").GetShort() : 0;
			int maxBurnTime = te.Has("MaxBurnTime") ? te.Get("MaxBurnTime").GetShort() : 0;
			int cookTime = te.Has("CookTime") ? te.Get("CookTime").GetShort() : 0;
			auto ent = std::make_shared<TileEntityFurnace>(pos, burnTime, maxBurnTime, cookTime);
			loadSlots(ent->inventory.slots);
			chunk->tileEntities.push_back(std::move(ent));
		} else if (id == "Trap") {
			auto ent = std::make_shared<TileEntityDispenser>(pos);
			loadSlots(ent->inventory.slots);
			chunk->tileEntities.push_back(std::move(ent));
		} else if (id == "Sign") {
			auto ent = std::make_shared<TileEntitySign>(pos);
			if (te.Has("Text1"))
				ent->text1 = te.Get("Text1").GetString();
			if (te.Has("Text2"))
				ent->text2 = te.Get("Text2").GetString();
			if (te.Has("Text3"))
				ent->text3 = te.Get("Text3").GetString();
			if (te.Has("Text4"))
				ent->text4 = te.Get("Text4").GetString();
			chunk->tileEntities.push_back(std::move(ent));
		} else if (id == "Music") {
			auto ent = std::make_shared<TileEntityNoteblock>(pos);
			if (te.Has("note"))
				ent->note = te.Get("note").GetByte();
			chunk->tileEntities.push_back(std::move(ent));
		} else if (id == "RecordPlayer") {
			auto ent = std::make_shared<TileEntityJukebox>(pos);
			if (te.Has("Record"))
				ent->recordItemId = ItemId(te.Get("Record").GetInt());
			chunk->tileEntities.push_back(std::move(ent));
		} else if (id == "MobSpawner") {
			auto ent = std::make_shared<TileEntityMobSpawner>(pos);
			if (te.Has("EntityId"))
				ent->entityId = te.Get("EntityId").GetString();
			if (te.Has("Delay"))
				ent->delay = te.Get("Delay").GetShort();
			chunk->tileEntities.push_back(std::move(ent));
		} else if (id == "Piston") {
			auto ent = std::make_shared<TileEntityPistonMoving>(pos);
			if (te.Has("blockId"))
				ent->storedBlock = BlockType(te.Get("blockId").GetInt());
			if (te.Has("blockData"))
				ent->storedMeta = uint8_t(te.Get("blockData").GetInt());
			if (te.Has("facing"))
				ent->orientation = te.Get("facing").GetInt();
			if (te.Has("progress"))
				ent->lastProgress = ent->progress = te.Get("progress").GetFloat();
			if (te.Has("extending"))
				ent->extending = te.Get("extending").GetByte() != 0;

			if (ent->orientation < 0 || ent->orientation > 5)
				ent->orientation = 0;

			chunk->tileEntities.push_back(std::move(ent));
		}
	}

	// The above stuff marks the chunk as dirty,
	// which results in it getting resaved immediately after loading.
	// This is quite wasteful and can harm storage devices long-term,
	// so it's probably best to not do that.
	chunk->isModified = false;

	return chunk;
}