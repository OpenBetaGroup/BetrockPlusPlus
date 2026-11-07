/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once
#include "chunk.h"
#include "subchunk.h"
#include "helpers/deflate_pool.h"
#include <vector>

namespace ChunkSerializer {
inline std::vector<uint8_t> Serialize(const Chunk& _chunk, int _xmin = 0, int _xmax = CHUNK_WIDTH, int _ymin = 0,
                                      int _ymax = CHUNK_HEIGHT, int _zmin = 0, int _zmax = CHUNK_WIDTH) {
	const int sizeX = _xmax - _xmin;
	const int sizeY = _ymax - _ymin;
	const int sizeZ = _zmax - _zmin;

	const int blocks = sizeX * sizeY * sizeZ;
	const int nibbles = (blocks + 1) / 2;
	const int total = blocks + nibbles * 3;

	// NOTE: This means each per-thread chunk serializer grows as big as its biggest allocation.
	// So for a long-running server, which gets abused to the max (via explosions, for example),
	// this'd be ~5MB in the worst case (128x128x128) per thread.
	// TODO: Maybe shrink these after a few ticks?
	// Most chunk serializations should only take a few KB on average!
	thread_local std::vector<uint8_t> raw;
	raw.assign(size_t(total), 0);
	uint8_t* blockData = raw.data();
	uint8_t* metaData = blockData + blocks;
	uint8_t* blockLight = metaData + nibbles;
	uint8_t* skyLight = blockLight + nibbles;

	auto packNibble = [](uint8_t& _byte, uint8_t _val, bool _high) {
		if (_high)
			_byte = uint8_t((_byte & 0x0F) | ((_val & 0x0F) << 4));
		else
			_byte = uint8_t((_byte & 0xF0) | (_val & 0x0F));
	};

	int i = 0;
	for (int x = _xmin; x < _xmax; x++) {
		for (int z = _zmin; z < _zmax; z++) {
			for (int y = _ymin; y < _ymax;) {
				const int slabEnd = std::min<int>(_ymax, (y | (SUB_CHUNK_SIZE - 1)) + 1);
				const SubChunk& sub = _chunk.subChunks[size_t(y >> 4)];

				const SubChunk::BlockLayer* blockLayer = sub.blocks.get();
				const SubChunk::NibbleLayer* metaLayer = sub.meta.get();
				const SubChunk::NibbleLayer* blockLightLayer = sub.blockLight.get();
				const SubChunk::NibbleLayer* skyLightLayer = sub.skyLight.get();
				const uint8_t blockFill = uint8_t(sub.blockFill);
				const uint8_t metaFill = sub.metaFill;
				const uint8_t blockLightFill = sub.blockLightFill;
				const uint8_t skyLightFill = sub.skyLightFill;

				for (; y < slabEnd; y++, i++) {
					const int idx = SubChunk::LocalIndex({ x, y, z });
					blockData[i] = blockLayer ? uint8_t(blockLayer->types[size_t(idx)]) : blockFill;
					// The buffer is zero-initialised, so a zero meta fill needs no write
					if (metaLayer)
						packNibble(metaData[i >> 1], metaLayer->Get(idx), i & 1);
					else if (metaFill != 0)
						packNibble(metaData[i >> 1], metaFill, i & 1);
					packNibble(blockLight[i >> 1],
					           blockLightLayer ? blockLightLayer->Get(idx) : blockLightFill, i & 1);
					packNibble(skyLight[i >> 1],
					           skyLightLayer ? skyLightLayer->Get(idx) : skyLightFill, i & 1);
				}
			}
		}
	}

	return Deflate::Compress(Deflate::Format::Zlib, raw, 1);
}
} // namespace ChunkSerializer