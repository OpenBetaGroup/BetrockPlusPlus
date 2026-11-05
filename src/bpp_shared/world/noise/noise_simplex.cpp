/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "noise_simplex.h"
#include "noise_generator.h"

#include <algorithm>
#include <cassert>

NoiseSimplex::NoiseSimplex() {
	Java::Random rand;
	InitPermTable(rand);
}

NoiseSimplex::NoiseSimplex(Java::Random& _rand) {
	InitPermTable(_rand);
}

void NoiseSimplex::InitPermTable(Java::Random& _rand) {
	NoiseGenerator::InitPermTable(_rand);
	for (int32_t i = 0; i < 512; ++i)
		permMod12[i] = permutations[i] % 12;
}

void NoiseSimplex::GenerateNoise(std::span<double> _noiseField, Vec2 _offset, Int32_2 _size, Vec2 _scale, double _amplitude) {
	const int32_t sizeX = _size.x;
	const int32_t sizeY = _size.y;

	if (sizeX <= 0 || sizeY <= 0) [[unlikely]]
		return;
	if (sizeY > CHUNK_WIDTH) [[unlikely]]
		return;

	const gen_float scaleX    = _scale.x;
	const gen_float scaleY    = _scale.y;
	const gen_float offsetX   = _offset.x;
	const gen_float offsetY   = _offset.y;
	const gen_float amplitude = gen_float(_amplitude * 70.0); // 70 normalizes simplex output

	double* const output = _noiseField.data();

	// The Y sample positions don't depend on X, so compute them once.
	// (The X-dependent skew means the rest can't be hoisted out of the loop.)
	std::array<gen_float, CHUNK_WIDTH> yPositions;
	for (int32_t y = 0; y < sizeY; ++y)
		yPositions[y] = (offsetY + gen_float(y)) * scaleY + coordinate.y;

	const uint8_t* const perm  = permutations;
	const uint8_t* const mod12 = permMod12;

	for (int32_t x = 0; x < sizeX; ++x) {
		const gen_float xPos = (offsetX + gen_float(x)) * scaleX + coordinate.x;

		// Output is laid out column-major: sizeY consecutive values per X.
		double* const column = output + static_cast<size_t>(x) * sizeY;

		for (int32_t y = 0; y < sizeY; ++y) {
			const gen_float yPos = yPositions[y];

			// 1. Skew input space to find which simplex cell we're in.
			const gen_float skew = (xPos + yPos) * SKEWING;
			const int32_t cellX = Wrap(xPos + skew);
			const int32_t cellY = Wrap(yPos + skew);

			// 2. Unskew the cell origin back to input space, and get the
			//    sample's offset from it (offset to the first corner).
			const gen_float unskew  = gen_float(cellX + cellY) * UNSKEWING;
			const gen_float originX = gen_float(cellX) - unskew;
			const gen_float originY = gen_float(cellY) - unskew;
			const gen_float dx0 = xPos - originX;
			const gen_float dy0 = yPos - originY;

			// 3. Pick the middle corner: the simplex is one of two triangles,
			//    depending on which offset is larger.
			const bool lowerTriangle = dx0 > dy0;
			const int32_t stepX = lowerTriangle ? 1 : 0;
			const int32_t stepY = lowerTriangle ? 0 : 1;

			// 4. Offsets from the sample to the middle and last corners.
			const gen_float dx1 = dx0 - gen_float(stepX) + UNSKEWING;
			const gen_float dy1 = dy0 - gen_float(stepY) + UNSKEWING;
			const gen_float dx2 = dx0 - 1.0 + 2.0 * UNSKEWING;
			const gen_float dy2 = dy0 - 1.0 + 2.0 * UNSKEWING;

			// 5. Hash each corner to a gradient index.
			const int32_t hashX = cellX & 255;
			const int32_t hashY = cellY & 255;
			const int32_t grad0 = mod12[hashX                + perm[hashY]];
			const int32_t grad1 = mod12[hashX + stepX        + perm[hashY + stepY]];
			const int32_t grad2 = mod12[hashX + 1            + perm[hashY + 1]];

			// 6. Sum the three corner contributions.
			const gen_float noise = Contribution(grad0, dx0, dy0)
								  + Contribution(grad1, dx1, dy1)
								  + Contribution(grad2, dx2, dy2);

			column[y] += double(noise * amplitude);
		}
	}
}