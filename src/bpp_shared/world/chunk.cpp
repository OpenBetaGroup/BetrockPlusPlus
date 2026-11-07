/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "chunk.h"
#include <algorithm>

int Chunk::GetHighestPoint() const {
	int highestPoint = 0;
	for (auto& i : heightMap) {
		if (i > highestPoint)
			highestPoint = i;
	}
	return highestPoint;
}

bool Chunk::CanBlockSeeSky(Int3 _pos) const {
	return _pos.y >= GetHeightValue({ _pos.x, _pos.z });
}

void Chunk::GenerateHeightMap() {
	for (int x = 0; x < CHUNK_WIDTH; x++)
		for (int z = 0; z < CHUNK_WIDTH; z++)
			GenerateHeightMapColumn({ x, z });
}

void Chunk::GenerateHeightMapColumn(Int2 _pos) {
	for (int y = CHUNK_HEIGHT - 1; y >= 0; y--) {
		if (Blocks::blockProperties[GetBlock({ _pos.x, y, _pos.z })].lightOpacity > 0) {
			SetHeightValue(_pos, uint8_t(y + 1));
			return;
		}
	}
	SetHeightValue(_pos, 0);
}

void Chunk::GenerateSkylightMap() {
	GenerateHeightMap();

	// Every slot that starts at or above the tallest column is open sky in every column, so light it in one go
	const int bulkStart = std::min<int>(CHUNK_HEIGHT,
	                                               (GetHighestPoint() + SUB_CHUNK_SIZE - 1) & ~(SUB_CHUNK_SIZE - 1));

	for (int i = bulkStart / SUB_CHUNK_SIZE; i < SUB_CHUNK_COUNT; i++)
		subChunks[size_t(i)].FillSkyLight(15);

	for (int x = 0; x < CHUNK_WIDTH; x++) {
		for (int z = 0; z < CHUNK_WIDTH; z++) {
			int height = GetHeightValue({ x, z });
			for (int y = bulkStart - 1; y >= height; y--)
				SetSkyLight({ x, y, z }, 15);
			int skyLight = 15;
			for (int y = height - 1; y >= 0; y--) {
				skyLight -= std::max<int>(1,
				                                     int(Blocks::blockProperties[GetBlock({ x, y, z })].lightOpacity));
				skyLight = std::max<int>(0, skyLight);
				SetSkyLight({ x, y, z }, uint8_t(skyLight));
			}
		}
	}
	isModified = true;
	Compact();
}

void Chunk::RelightColumn(Int2 _pos) {
	GenerateHeightMapColumn(_pos);
	int height = GetHeightValue(_pos);

	for (int y = CHUNK_HEIGHT - 1; y >= height; y--)
		SetSkyLight({ _pos.x, y, _pos.z }, 15);
}

void Chunk::RecalculateSkyLightColumn(Int2 _pos) {
	GenerateHeightMapColumn(_pos);
	const int height = GetHeightValue(_pos);

	for (int y = CHUNK_HEIGHT - 1; y >= height; y--)
		SetSkyLight({ _pos.x, y, _pos.z }, 15);

	int skyLight = 15;
	for (int y = height - 1; y >= 0; y--) {
		skyLight -= std::max<int>(1,
		                                     int(Blocks::blockProperties[GetBlock({ _pos.x, y, _pos.z })].lightOpacity));
		skyLight = std::max<int>(0, skyLight);
		SetSkyLight({ _pos.x, y, _pos.z }, uint8_t(skyLight));
	}
}

void Chunk::Clear() {
	isTerrainPopulated = false;
	isModified = false;
	climateBaked = false;
	for (auto& sub : subChunks)
		sub.Reset();
	std::memset(heightMap, 0, sizeof(heightMap));
	std::memset(temperature, 0, sizeof(temperature));
	std::memset(humidity, 0, sizeof(humidity));
}

namespace {
std::unique_ptr<SubChunk::NibbleLayer> MakeNibbleLayer(uint8_t _fill) {
	// Every byte is about to be overwritten, so skip the zeroing
	auto layer = std::make_unique_for_overwrite<SubChunk::NibbleLayer>();
	const uint8_t packed = uint8_t(((_fill & 0xF) << 4) | (_fill & 0xF));
	layer->data.fill(packed);
	return layer;
}

// A nibble layer is uniform when the first byte has matching halves and every other byte equals it
bool CompactNibbleLayer(std::unique_ptr<SubChunk::NibbleLayer>& _layer, uint8_t& _fill) {
	if (!_layer)
		return false;

	const uint8_t first = _layer->Get(0);
	for (size_t i = 1; i < SubChunk::VOLUME; i++) {
		if (_layer->Get(i) != first)
			return false;
	}

	_fill = first;
	_layer.reset();
	return true;
}

template <typename T>
std::unique_ptr<T> CopyLayer(const std::unique_ptr<T>& _src) {
	return _src ? std::make_unique<T>(*_src) : nullptr;
}
} // namespace

void SubChunk::AllocBlocks() {
	blocks = std::make_unique_for_overwrite<BlockLayer>();
	blocks->types.fill(blockFill);
	blocks->counts.fill(0);
	blocks->counts[size_t(blockFill)] = uint16_t(VOLUME);
}

void SubChunk::AllocMeta() {
	meta = MakeNibbleLayer(metaFill);
}

void SubChunk::AllocBlockLight() {
	blockLight = MakeNibbleLayer(blockLightFill);
}

void SubChunk::AllocSkyLight() {
	skyLight = MakeNibbleLayer(skyLightFill);
}

void SubChunk::FillRegion(Int3 _a, Int3 _b, BlockType _type, uint8_t _meta) {
	_meta &= 0xF;
	const bool fullFootprint = _a.x == 0 && _b.x == SIZE - 1 && _a.z == 0 && _b.z == SIZE - 1;

	if (fullFootprint && _a.y == 0 && _b.y == SIZE - 1) {
		FillBlocks(_type);
		FillMeta(_meta);
		return;
	}

	if (blocks || _type != blockFill) {
		if (!blocks)
			AllocBlocks();
		if (fullFootprint) {
			FillBlockSpan(size_t(_a.y) * SIZE * SIZE, size_t(_b.y - _a.y + 1) * SIZE * SIZE, _type);
		} else {
			for (int y = _a.y; y <= _b.y; y++)
				for (int z = _a.z; z <= _b.z; z++)
					FillBlockSpan(size_t(LocalIndex({ _a.x, y, z })), size_t(_b.x - _a.x + 1), _type);
		}
	}

	if (meta || _meta != metaFill) {
		if (!meta)
			AllocMeta();
		if (fullFootprint) {
			const uint8_t packed = uint8_t(_meta | (_meta << 4));
			std::memset(meta->data.data() + size_t(_a.y) * SIZE * SIZE / 2, packed,
            	size_t(_b.y - _a.y + 1) * SIZE * SIZE / 2);
		} else {
			for (int y = _a.y; y <= _b.y; y++)
				for (int z = _a.z; z <= _b.z; z++)
					for (int x = _a.x; x <= _b.x; x++)
						meta->Set(LocalIndex({ x, y, z }), _meta);
		}
	}
}

namespace {
// Normalizes and clamps a chunk-local box. Returns false if nothing of it is inside the chunk.
bool ClampChunkBox(Int3 _a, Int3 _b, Int3& _lo, Int3& _hi) {
	const int loX = std::min<int>(_a.x, _b.x), hiX = std::max<int>(_a.x, _b.x);
	const int loY = std::min<int>(_a.y, _b.y), hiY = std::max<int>(_a.y, _b.y);
	const int loZ = std::min<int>(_a.z, _b.z), hiZ = std::max<int>(_a.z, _b.z);
	if (hiX < 0 || loX >= CHUNK_WIDTH || hiY < 0 || loY >= CHUNK_HEIGHT || hiZ < 0 || loZ >= CHUNK_WIDTH)
		return false;
	_lo = { std::max<int>(loX, 0), std::max<int>(loY, 0), std::max<int>(loZ, 0) };
	_hi = { std::min<int>(hiX, CHUNK_WIDTH - 1), std::min<int>(hiY, CHUNK_HEIGHT - 1),
		    std::min<int>(hiZ, CHUNK_WIDTH - 1) };
	return true;
}
} // namespace

void Chunk::FillRegion(Int3 _a, Int3 _b, BlockType _type, uint8_t _meta) {
	Int3 lo, hi;
	if (!ClampChunkBox(_a, _b, lo, hi))
		return;
	for (int sub = lo.y >> 4; sub <= (hi.y >> 4); sub++) {
		const int base = sub * SUB_CHUNK_SIZE;
		const int y0 = std::max<int>(lo.y, base) - base;
		const int y1 = std::min<int>(hi.y, base + SUB_CHUNK_SIZE - 1) - base;
		subChunks[size_t(sub)].FillRegion({ lo.x, y0, lo.z }, { hi.x, y1, hi.z }, _type, _meta);
	}
	isModified = true;
}

void Chunk::FillBlockLightRegion(Int3 _a, Int3 _b, uint8_t _val) {
	Int3 lo, hi;
	if (!ClampChunkBox(_a, _b, lo, hi))
		return;
	_val &= 0xF;
	const bool fullFootprint = lo.x == 0 && hi.x == CHUNK_WIDTH - 1 && lo.z == 0 && hi.z == CHUNK_WIDTH - 1;
	for (int sub = lo.y >> 4; sub <= (hi.y >> 4); sub++) {
		const int base = sub * SUB_CHUNK_SIZE;
		const int y0 = std::max<int>(lo.y, base) - base;
		const int y1 = std::min<int>(hi.y, base + SUB_CHUNK_SIZE - 1) - base;
		SubChunk& s = subChunks[size_t(sub)];
		if (fullFootprint && y0 == 0 && y1 == SUB_CHUNK_SIZE - 1) {
			s.FillBlockLight(_val);
			continue;
		}
		for (int y = y0; y <= y1; y++)
			for (int z = lo.z; z <= hi.z; z++)
				for (int x = lo.x; x <= hi.x; x++)
					s.SetBlockLight(SubChunk::LocalIndex({ x, y, z }), _val);
	}
	isModified = true;
}

bool SubChunk::CompactBlocks() {
	if (!blocks)
		return false;

	const BlockType first = blocks->types[0];
	if (blocks->counts[size_t(first)] != VOLUME)
		return false;

	blockFill = first;
	blocks.reset();
	return true;
}

bool SubChunk::CompactMeta() {
	uint8_t fill;
	if (!CompactNibbleLayer(meta, fill))
		return false;
	metaFill = fill;
	return true;
}

bool SubChunk::CompactBlockLight() {
	uint8_t fill;
	if (!CompactNibbleLayer(blockLight, fill))
		return false;
	blockLightFill = fill;
	return true;
}

bool SubChunk::CompactSkyLight() {
	uint8_t fill;
	if (!CompactNibbleLayer(skyLight, fill))
		return false;
	skyLightFill = fill;
	return true;
}

bool SubChunk::Compact() {
	const bool freedBlocks = CompactBlocks();
	const bool freedMeta = CompactMeta();
	const bool freedBlockLight = CompactBlockLight();
	const bool freedSkyLight = CompactSkyLight();
	return freedBlocks || freedMeta || freedBlockLight || freedSkyLight;
}

void SubChunk::Reset() {
	blocks.reset();
	meta.reset();
	blockLight.reset();
	skyLight.reset();
	blockFill = BLOCK_AIR;
	metaFill = 0;
	blockLightFill = 0;
	skyLightFill = 15;
}

void SubChunk::CopyFrom(const SubChunk& _other) {
	blocks = CopyLayer(_other.blocks);
	meta = CopyLayer(_other.meta);
	blockLight = CopyLayer(_other.blockLight);
	skyLight = CopyLayer(_other.skyLight);
	blockFill = _other.blockFill;
	metaFill = _other.metaFill;
	blockLightFill = _other.blockLightFill;
	skyLightFill = _other.skyLightFill;
}

bool Chunk::TryToCompactSubChunk(int _index) {
	assert(_index >= 0 && _index < SUB_CHUNK_COUNT);
	return subChunks[size_t(_index)].Compact();
}

void Chunk::Compact() {
	for (auto& sub : subChunks)
		sub.Compact();
}

void Chunk::CopyStorageFrom(const Chunk& _other) {
	for (size_t i = 0; i < subChunks.size(); i++)
		subChunks[i].CopyFrom(_other.subChunks[i]);
}
