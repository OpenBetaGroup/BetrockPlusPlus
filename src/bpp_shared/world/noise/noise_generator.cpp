/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 * 
*/

#include "noise_generator.h"
#include <utility>

void NoiseGenerator::InitPermTable(Java::Random& _rand) {
	coordinate.x = _rand.NextDouble() * 256.0;
	coordinate.y = _rand.NextDouble() * 256.0;
	coordinate.z = _rand.NextDouble() * 256.0;

	for (int32_t i = 0; i < 256; ++i) {
		permutations[i] = i;
	}

	for (int32_t i = 0; i < 256; ++i) {
		int32_t j = _rand.NextInt(256 - i) + i;
		std::swap(permutations[i], permutations[j]);
		permutations[i + 256] = permutations[i];
	}
}

NoiseGenerator::NoiseGenerator() {}

NoiseGenerator::NoiseGenerator(Java::Random& /*_rand*/) {}