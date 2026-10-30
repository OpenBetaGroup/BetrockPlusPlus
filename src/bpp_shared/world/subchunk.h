/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once

// A 16x16x16 slice of a Chunk. Chunks are stored as a vertical stack of these.
struct SubChunk {
	static constexpr int SIZE = SUB_CHUNK_SIZE;
	static constexpr int VOLUME = SIZE * SIZE * SIZE;

	struct BlockLayer {
		std::array<BlockType, VOLUME> types;
		std::array<uint16_t, BLOCK_MAX> counts;
	};

	using NibbleLayer = PackedArray<VOLUME, 4>;

	// Null = uniform, see the matching fill below
	std::unique_ptr<BlockLayer> blocks = nullptr;
	// Invariant: non-null exactly when blocks is non-null
	std::unique_ptr<NibbleLayer> meta = nullptr;
	std::unique_ptr<NibbleLayer> blockLight = nullptr;
	std::unique_ptr<NibbleLayer> skyLight = nullptr;

	// What a null layer reads as. Defaults to air, no meta, no block light and full sky light.
	BlockType blockFill = BLOCK_AIR;
	uint8_t metaFill : 4 = 0;
	uint8_t blockLightFill : 4 = 0;
	uint8_t skyLightFill : 4 = 15;

	// Takes chunk-relative coordinates, only the low 4 bits of y are used.
	static inline int LocalIndex(Int3 _pos) {
		return ((_pos.y & (SIZE - 1)) * SIZE * SIZE) + (_pos.z * SIZE) + _pos.x;
	}

	// Reads. Take a local index, see LocalIndex()
	inline BlockType GetBlock(int _idx) const {
		return blocks ? blocks->types[size_t(_idx)] : blockFill;
	}
	inline uint8_t GetMeta(int _idx) const {
		return meta ? meta->Get(_idx) : metaFill;
	}
	inline uint8_t GetBlockLight(int _idx) const {
		return blockLight ? blockLight->Get(_idx) : blockLightFill;
	}
	inline uint8_t GetSkyLight(int _idx) const {
		return skyLight ? skyLight->Get(_idx) : skyLightFill;
	}

	// Writes. A layer is only allocated when the value differs from what it currently reads as
	inline void SetBlock(int _idx, BlockType _val) {
		if (!blocks) {
			if (_val == blockFill)
				return;
			AllocBlocks();
		}
		BlockType& slot = blocks->types[size_t(_idx)];
		if (slot == _val)
			return;
		--blocks->counts[size_t(slot)];
		++blocks->counts[size_t(_val)];
		slot = _val;
	}
	inline void SetMeta(int _idx, uint8_t _val) {
		_val &= 0x0Fu;
		if (!meta) {
			if (_val == metaFill)
				return;
			AllocMeta();
		}
		meta->Set(_idx, _val);
	}
	inline void SetBlockLight(int _idx, uint8_t _val) {
		_val &= 0x0Fu;
		if (!blockLight) {
			if (_val == blockLightFill)
				return;
			AllocBlockLight();
		}
		blockLight->Set(_idx, _val);
	}
	inline void SetSkyLight(int _idx, uint8_t _val) {
		_val &= 0x0Fu;
		if (!skyLight) {
			if (_val == skyLightFill)
				return;
			AllocSkyLight();
		}
		skyLight->Set(_idx, _val);
	}
	// Sets every voxel in the slice, releasing that layer's storage
	inline void FillBlocks(BlockType _val) {
		blocks.reset();
		blockFill = _val;
	}
	inline void FillMeta(uint8_t _val) {
		meta.reset();
		metaFill = _val & 0x0Fu;
	}
	inline void FillBlockLight(uint8_t _val) {
		blockLight.reset();
		blockLightFill = _val & 0x0Fu;
	}
	inline void FillSkyLight(uint8_t _val) {
		skyLight.reset();
		skyLightFill = _val & 0x0Fu;
	}
	// Recounts the block layer from scratch. Only needed if something writes to block types
	void RebuildBlockCounts() {
		if (!blocks)
			return;
		blocks->counts.fill(0);
		for (BlockType b : blocks->types)
			++blocks->counts[size_t(b)];
	}
	// Number of voxels of this type in the slice
	inline int CountBlock(BlockType _type) const {
		if (!blocks)
			return blockFill == _type ? VOLUME : 0;
		return blocks->counts[size_t(_type)];
	}
	// Check if this block exists anywhere in the subchunk. O(1)
	inline bool HasBlock(BlockType _type) const {
		return CountBlock(_type) != 0;
	}
	// Overwrites a contiguous run of the block layer, keeping the counts in sync. Layer must be allocated.
	inline void FillBlockSpan(size_t _start, size_t _count, BlockType _type) {
		BlockType* p = blocks->types.data() + _start;
		for (size_t i = 0; i < _count; i++)
			--blocks->counts[size_t(p[i])];
		std::fill_n(p, _count, _type);
		blocks->counts[size_t(_type)] += uint16_t(_count);
	}
	// Finds blocks in subchunk-local space
	// Appends the subchunk-local position of every voxel of this type. Returns how many were added.
	inline int FindBlocks(BlockType _type, std::vector<Int3>& _out, int _yOffset = 0) const {
		// If we have none of that block skip
		const int count = CountBlock(_type);
		if (count == 0)
			return 0;
		_out.reserve(_out.size() + size_t(count));
		// Just blockFill, so we send every block coordinate
		if (!blocks) {
			for (int i = 0; i < VOLUME; i++)
				_out.push_back({ i & 15, _yOffset + (i >> 8), (i >> 4) & 15 });
			return count;
		}
		// Look for positions of blocks
		int found = 0;
		const BlockLayer& layer = *blocks;
		for (int i = 0; i < VOLUME && found < count; i++) {
			if (layer.types[size_t(i)] != _type)
				continue;
			_out.push_back({ i & 15, _yOffset + (i >> 8), (i >> 4) & 15 });
			found++;
		}
		return found;
	}
	inline std::vector<Int3> FindBlocks(BlockType _type) const {
		std::vector<Int3> out;
		FindBlocks(_type, out);
		return out;
	}
	void FillRegion(Int3 _posA, Int3 _posB, BlockType _type = BLOCK_AIR, uint8_t _meta = 0);

	// Allocates a layer holding its current fill value in every voxel. Only meant for null layers
	void AllocBlocks();
	void AllocMeta();
	void AllocBlockLight();
	void AllocSkyLight();

	// Frees a layer if every voxel in it holds the same value, moving that value into the fill.
	// Returns true if the layer was freed.
	bool CompactBlocks();
	bool CompactMeta();
	bool CompactBlockLight();
	bool CompactSkyLight();
	// Tries every layer, each on its own. Returns true if at least one was freed.
	bool Compact();

	// Back to all-default: air, no meta, no block light, full sky light. Frees every layer
	void Reset();
	// Deep copies every layer
	void CopyFrom(const SubChunk& _other);

	// Bytes of layer storage currently allocated, not counting sizeof(SubChunk) itself
	inline size_t GetHeapUsage() const {
		return (blocks ? sizeof(BlockLayer) : 0) + (meta ? sizeof(NibbleLayer) : 0) +
		       (blockLight ? sizeof(NibbleLayer) : 0) + (skyLight ? sizeof(NibbleLayer) : 0);
	}
	inline bool HasAnyLayer() const {
		return blocks || meta || blockLight || skyLight;
	}
};