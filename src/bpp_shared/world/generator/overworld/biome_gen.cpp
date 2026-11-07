/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "biome_gen.h"
#include "chunk.h"
#include <algorithm>

// This one does nothing. DO NOT USE!
BiomeGenerator::BiomeGenerator() {}

/**
 * @brief Construct a new Beta 1.7.3 Biome
 * 
 * @param seed The world seed that the biome-generator will use
 */
BiomeGenerator::BiomeGenerator(int64_t _seed) {
	// Init Biome Noise
	Java::Random randTemp = Java::Random(std::bit_cast<int64_t>(static_cast<uint64_t>(_seed) * 9871ULL));
	Java::Random randHum = Java::Random(std::bit_cast<int64_t>(static_cast<uint64_t>(_seed) * 39811ULL));
	Java::Random randWeird = Java::Random(std::bit_cast<int64_t>(static_cast<uint64_t>(_seed) * 543321ULL));
	temperatureNoiseGen = NoiseOctavesSimplex(randTemp, 4);
	humidityNoiseGen = NoiseOctavesSimplex(randHum, 4);
	weirdnessNoiseGen = NoiseOctavesSimplex(randWeird, 2);
}

/**
 * @brief Generate Biomes based on simplex noise and updates the temperature, humidity and weirdness maps
 * 
 * @param biomeMap The biome map the final Biome values should be written to
 * @param temperature The temperature map that'll be used/written to during generation
 * @param humidity The humidity map that'll be used/written to during generation
 * @param weirdness The weirdness map that'll be used/written to during generation
 * @param blockPos The x,z block-space coordindate of the chunk
 * @param max The size of the area that'll be generated (16x16 by default)
 */
void BiomeGenerator::GenerateBiomeMap(PackedArray<CHUNK_AREA, 4>& _biomeMap, std::span<double> _temperature,
                                      std::span<double> _humidity, std::span<double> _weirdness, Int2 _blockPos) {
	static constexpr Int32_2 MAX_AREA{ CHUNK_WIDTH, CHUNK_WIDTH };
	// TODO: Probably unnecessary? We always generate them with the proper size :p
	static constexpr size_t K_COUNT = size_t(CHUNK_WIDTH) * size_t(CHUNK_WIDTH);
	if (_temperature.size() < K_COUNT || _humidity.size() < K_COUNT || _weirdness.size() < K_COUNT)
		return;
	this->temperatureNoiseGen.GenerateOctaves(_temperature, _blockPos, MAX_AREA, temperatureBaseScale,
	                                          temperatureFactor);
	this->humidityNoiseGen.GenerateOctaves(_humidity, _blockPos, MAX_AREA, humidityBaseScale, humidityFactor);
	this->weirdnessNoiseGen.GenerateOctaves(_weirdness, _blockPos, MAX_AREA, weirdnessBaseScale, weirdnessFactor);
	size_t index = 0;
	// Iterate over each block column
	for (int32_t iX = 0; iX < CHUNK_WIDTH; ++iX) {
		for (int32_t iZ = 0; iZ < CHUNK_WIDTH; ++iZ) {
			const double weird = _weirdness[index] * 1.1 + 0.5;
			double scale = 0.01;
			double limit = 1.0 - scale;
			double temp = (_temperature[index] * 0.15 + 0.7) * limit + weird * scale;
			scale = 0.002;
			limit = 1.0 - scale;
			double humi = (_humidity[index] * 0.15 + 0.5) * limit + weird * scale;
			temp = 1.0 - (1.0 - temp) * (1.0 - temp);
			// Limit values to 0.0 - 1.0
			temp = std::clamp(temp, 0.0, 1.0);
			humi = std::clamp(humi, 0.0, 1.0);
			// Write the temperature and humidity values back
			_temperature[index] = temp;
			_humidity[index] = humi;
			// Get the biome from the lookup
			_biomeMap.Set(index, GetBiomeFromLookup(float(temp), float(humi)));
			index++;
		}
	}
}

Biome BiomeGenerator::GetBiomeAtPoint(Int2 _worldPos) {
	// TODO: Maybe add thing for sampling at a single point without this?
	double temp[1], humi[1], weird[1];
	this->temperatureNoiseGen.GenerateOctaves(temp, Int2{ _worldPos.x, _worldPos.y }, Int32_2{ 1, 1 },
	                                          temperatureBaseScale, temperatureFactor);
	this->humidityNoiseGen.GenerateOctaves(humi, Int2{ _worldPos.x, _worldPos.y }, Int32_2{ 1, 1 }, humidityBaseScale,
	                                       humidityFactor);
	this->weirdnessNoiseGen.GenerateOctaves(weird, Int2{ _worldPos.x, _worldPos.y }, Int32_2{ 1, 1 },
	                                        weirdnessBaseScale, weirdnessFactor);
	const double w = weird[0] * 1.1 + 0.5;
	double t = (temp[0] * 0.15 + 0.7) * 0.99 + w * 0.01;
	double h = (humi[0] * 0.15 + 0.5) * 0.998 + w * 0.002;
	t = 1.0 - (1.0 - t) * (1.0 - t);
	t = std::clamp(t, 0.0, 1.0);
	h = std::clamp(h, 0.0, 1.0);
	return GetBiomeFromLookup(float(t), float(h));
}

/**
 * @brief Generates the temperature map values
 * 
 * @param temperature The temperature map that'll be used/written to during generation
 * @param weirdness The weirdness map that'll be used/written to during generation
 * @param blockPos The x,z block-space coordindate of the chunk
 * @param max The size of the area that'll be generated (16x16 by default)
 */
void BiomeGenerator::GenerateTemperature(std::span<double> _temperature, std::span<double> _weirdness, Int2 _blockPos,
                                         Int2 _max) {
	const size_t count = size_t(_max.x) * size_t(_max.y);
	if (_temperature.size() < count || _weirdness.size() < count)
		return;
	this->temperatureNoiseGen.GenerateOctaves(_temperature, _blockPos, _max, temperatureBaseScale, temperatureFactor);
	this->weirdnessNoiseGen.GenerateOctaves(_weirdness, _blockPos, _max, weirdnessBaseScale, weirdnessFactor);
	size_t index = 0;
	// Iterate over each block column
	for (int32_t x = 0; x < _max.x; ++x) {
		for (int32_t z = 0; z < _max.y; ++z) {
			const double weird = _weirdness[index] * 1.1 + 0.5;
			const double scale = 0.01;
			const double limit = 1.0 - scale;
			double temp = (_temperature[index] * 0.15 + 0.7) * limit + weird * scale;
			temp = 1.0 - (1.0 - temp) * (1.0 - temp);
			// Limit values to 0.0 - 1.0
			temp = std::clamp(temp, 0.0, 1.0);
			// Write the temperature values back
			_temperature[index] = temp;
			++index;
		}
	}
}
