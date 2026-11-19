/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/
#pragma once
#include "../../noise/noise_octaves_perlin.h"
#include "../generator.h"
#include "../shared/cave_gen.h"
#include "chunk.h"
#include "constants.h"
#include "numeric_structs.h"
#include "world_wrapper.h"
#include <cstddef>
#include <cstdint>

/**
 * @brief A faithful reimplementation of the Beta 1.7.3 Nether Generator
 * 
 */
class NetherGenerator : public Generator {
private:
	// Perlin Noise Generators
	NoiseOctavesPerlin lowNoiseGen;
	NoiseOctavesPerlin highNoiseGen;
	NoiseOctavesPerlin selectorNoiseGen;
	NoiseOctavesPerlin sandGravelNoiseGen;
	NoiseOctavesPerlin stoneNoiseGen;
	NoiseOctavesPerlin continentalnessNoiseGen;
	NoiseOctavesPerlin depthNoiseGen;

	// Stored noise Fields (fixed buffers — no per-chunk heap traffic on the gen hot path)
	static constexpr Int3 MAX{ CHUNK_WIDTH / 4 + 1, CHUNK_HEIGHT / 8 + 1, CHUNK_WIDTH / 4 + 1 };
	static constexpr size_t MAX_AREA = (MAX.x * MAX.z);
	static constexpr size_t MAX_VOLUME = (MAX.x * MAX.y * MAX.z);
	double terrainNoiseField[MAX_VOLUME];
	double lowNoiseField[MAX_VOLUME];
	double highNoiseField[MAX_VOLUME];
	double selectorNoiseField[MAX_VOLUME];
	double continentalnessNoiseField[MAX_AREA];
	double depthNoiseField[MAX_AREA];

	double sandNoise[CHUNK_WIDTH * CHUNK_WIDTH];
	double gravelNoise[CHUNK_WIDTH * CHUNK_WIDTH];
	double stoneNoise[CHUNK_WIDTH * CHUNK_WIDTH];

	// Cave Gen
	CaveGenerator caver;

	void GenerateTerrain(Chunk& _chunk);
	void GenerateTerrainNoise(Int3 _cpos, Int3 _max);
	void ReplaceBlocksForBiome(Chunk& _chunk);

public:
	NetherGenerator(int64_t _seed);
	~NetherGenerator() = default;
	void GenerateChunk(Chunk& _chunk) override;
	bool PopulateChunk(Chunk& _chunk, WorldWrapper& _world) override;
};