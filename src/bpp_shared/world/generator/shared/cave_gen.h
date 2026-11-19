/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#pragma once

#include "chunk.h"
#include "java_random.h"
#include "numeric_structs.h"
#include <cstdint>

/**
 * @brief Used to carve caves into the world
 *
 */
class CaveGenerator {
private:
	static constexpr int32_t M_CARVE_EXTENT_LIMIT = 8;
	Java::Random rand = Java::Random();

public:
	CaveGenerator(bool _isNetherCave = false) : isNetherCave(_isNetherCave) {}
	void GenerateCavesForChunk(Chunk& _chunk, int64_t _seed);
	void GenerateCaves(Chunk& _chunk, Int2 _chunkOffset);
	void CarveCave(Chunk& _chunk, Vec3 _offset);
	void CarveCave(Chunk& _chunk, Vec3 _offset, float _tunnelRadius, float _carveYaw, float _carvePitch,
	               int32_t _tunnelStep, int32_t _tunnelLength, double _verticalScale);
	bool isNetherCave = false;
};