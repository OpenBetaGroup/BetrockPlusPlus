/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#pragma once
#include "./shared/feature_gen.h"
#include <cstdint>

/**
 * @brief The base generator class
 * 
 */
class Generator {
protected:
	Java::Random rand;
	int64_t seed = 0;

public:
	Generator(int64_t _seed);
	Generator() = default;
	virtual ~Generator() = default;
	virtual void GenerateChunk(Chunk& _chunk);
	virtual bool PopulateChunk(Chunk& _chunk, WorldWrapper& _world);
};