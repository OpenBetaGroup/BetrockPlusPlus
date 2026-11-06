/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#pragma once
#include "biomes.h"
#include "constants.h"
#include "helpers/packed_array.h"
#include "noise_octaves_simplex.h"
#include <span>

/**
 * @brief A faithful reimplementation of the Beta 1.7.3 biome generator
 * 
 */
class BiomeGenerator {
private:
	// Simplex Noise Generators
	NoiseOctavesSimplex temperatureNoiseGen;
	NoiseOctavesSimplex humidityNoiseGen;
	NoiseOctavesSimplex weirdnessNoiseGen;
	// Always the same
	static constexpr Vec2 temperatureBaseScale = {0.025f, 0.025f};
	static constexpr double temperatureFactor = 1.0 / 4.0;
	static constexpr Vec2 humidityBaseScale = {0.05f, 0.05f};
	static constexpr double humidityFactor = 1.0 / 3.0;
	static constexpr Vec2 weirdnessBaseScale = {0.25f, 0.25f};
	static constexpr double weirdnessFactor = 10.0 / 17.0;

public:
	BiomeGenerator();
	BiomeGenerator(int64_t _seed);
	Biome GetBiomeAtPoint(Int2 _worldPos);
	void GenerateBiomeMap(PackedArray<CHUNK_AREA, 4>& _biomeMap, std::span<double> _temperature,
	                      std::span<double> _humidity, std::span<double> _weirdness, Int2 _blockPos);
	void GenerateTemperature(std::span<double> _temperature, std::span<double> _weirdness, Int2 _chunkPos, Int2 _max);
	
	static bool BiomeHasRain(Biome _biome) {
		switch (_biome) {
		case BIOME_DESERT:
		case BIOME_ICEDESERT:
		case BIOME_HELL:
		case BIOME_SKY:
			return false;
		default:
			return true;
		}
	}

	static bool BiomeHasSnow(Biome _biome) {
		switch (_biome) {
		case BIOME_TAIGA:
		case BIOME_TUNDRA:
		case BIOME_ICEDESERT:
			return true;
		default:
			return false;
		}
	}

	static bool BiomeHasLightning(Biome _biome) {
		return BiomeHasRain(_biome) && !BiomeHasSnow(_biome);
	}
};
