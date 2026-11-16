/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/
#pragma once
#include "../generator.h"
#include "../noise/noise_octaves_perlin.h"
#include "../shared/cave_gen.h"
#include "../shared/feature_gen.h"
#include "biomes.h"
#include "generator/overworld/biome_gen.h"

/**
 * @brief A faithful reimplementation of the Beta 1.7.3 Overworld Generator
 * 
 */
class OverworldGenerator : public Generator {
private:
	// Perlin Noise Generators
	NoiseOctavesPerlin lowNoiseGen;
	NoiseOctavesPerlin highNoiseGen;
	NoiseOctavesPerlin selectorNoiseGen;
	NoiseOctavesPerlin sandGravelNoiseGen;
	NoiseOctavesPerlin stoneNoiseGen;
	NoiseOctavesPerlin continentalnessNoiseGen;
	NoiseOctavesPerlin depthNoiseGen;
	NoiseOctavesPerlin treeDensityNoiseGen;

	// Stored noise Fields
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

	PackedArray<CHUNK_AREA, 4> biomeMap;
	double temperature[CHUNK_AREA];
	double humidity[CHUNK_AREA];
	double weirdness[CHUNK_AREA];

	// Cave Gen
	CaveGenerator caver;

	// Reused biome generator
	BiomeGenerator biomeGen;

	void GenerateTerrain(Chunk& _chunk);
	void GenerateTerrainNoise(Int3 _cpos, Int3 _max);
	void ReplaceBlocksForBiome(Chunk& _chunk);
	Biome GetBiomeAt(Int2 _worldPos);
	void GenerateTreeForBiome(WorldWrapper& _world, Java::Random& _rand, Int3 _pos, Biome _biome);

public:
	OverworldGenerator(int64_t _seed);
	~OverworldGenerator() = default;
	void GenerateChunk(Chunk& _chunk) override;
	bool PopulateChunk(Chunk& _chunk, WorldWrapper& _world) override;
};
