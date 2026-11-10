/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#include "dimension.h"

Biome BiomeGetter::GetBiomeAtPoint(Int2 _worldPos) {
	return Biome::BIOME_NONE;
}

void BiomeGetter::GenerateBiomeMap(PackedArray<CHUNK_AREA, 4>& _biomeMap, std::span<double> _temperature,
                                      std::span<double> _humidity, std::span<double> _weirdness, Int2 _blockPos) {
	size_t index = 0;
	// Iterate over each block column
	for (int32_t iX = 0; iX < CHUNK_WIDTH; ++iX) {
		for (int32_t iZ = 0; iZ < CHUNK_WIDTH; ++iZ) {
			// Write the temperature and humidity values back
			_temperature[index] = 0.0;
			_humidity[index] = 0.0;
			_biomeMap.Set(index, BIOME_NONE);
			index++;
		}
	}
}