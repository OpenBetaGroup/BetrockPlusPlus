/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 * 
*/

#pragma once
#include "blocks.h"
#include <array>
#include <cstddef>
#include <cstdint>

enum Biome : uint8_t {
	BIOME_NONE = 0,
	BIOME_RAINFOREST = 1,
	BIOME_SWAMPLAND = 2,
	BIOME_SEASONALFOREST = 3,
	BIOME_FOREST = 4,
	BIOME_SAVANNA = 5,
	BIOME_SHRUBLAND = 6,
	BIOME_TAIGA = 7,
	BIOME_DESERT = 8,
	BIOME_PLAINS = 9,
	BIOME_ICEDESERT = 10,
	BIOME_TUNDRA = 11,
	BIOME_HELL = 12,
	BIOME_SKY = 13
};

Biome GetBiome(float _temperature, float _humidity);
Biome GetBiomeFromLookup(float _temperature, float _humidity);

BlockType GetTopBlock(Biome _biome);
BlockType GetFillerBlock(Biome _biome);

#define BIOME_LUT_SIZE 64 * 64

/**
 * @brief Generates the Biome LUT that is used in b1.7.3
 * 
 */
inline std::array<Biome, BIOME_LUT_SIZE> biomeLut;

inline struct BiomeLUTInitializer {
	BiomeLUTInitializer() {
		for (std::size_t temp = 0; temp < 64; ++temp) {
			for (std::size_t humi = 0; humi < 64; ++humi) {
				biomeLut[temp + humi * 64] = GetBiome(float(temp) / 63.0f, float(humi) / 63.0f);
			}
		}
	}
} biomeLutInitializer;