/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

// A recreation of the the Infdev 20100227-1433 Perlin noise function
#pragma once
#include "../../quantized_types.h"
#include "java_math.h"
#include "java_random.h"
#include "noise_generator.h"
#include "numeric_structs.h"
#include <cstdint>
#include <span>

/**
 * @brief A faithful reimplementation of the Beta-era simplex noise generator, often used for Biome generation
 * 
 */
class NoiseSimplex : public NoiseGenerator {
private:
	uint8_t permMod12[512];
	void InitPermTable(Java::Random& _rand) override;

	static constexpr int8_t GRADIENT_X[12] = { 1, -1, 1, -1, 1, -1, 1, -1, 0, 0, 0, 0 };
	static constexpr int8_t GRADIENT_Y[12] = { 1, 1, -1, -1, 0, 0, 0, 0, 1, -1, 1, -1 };
	// Note: MSVC and older versions GCC do not support
	// "sqrt" being used in constexpr, reason we do this instead
	static constexpr gen_float SKEWING = 0.36602540378443860;   // 0.5 * (sqrt(3.0) - 1.0)
	static constexpr gen_float UNSKEWING = 0.21132486540518713; // (3.0 - sqrt(3.0)) / 6.0
public:
	NoiseSimplex();
	NoiseSimplex(Java::Random& _rand);
	~NoiseSimplex() override {}
	void GenerateNoise(std::span<double> _noiseField, Vec2 _offset, Int32_2 _size, Vec2 _scale, double _amplitude);

private:
	static constexpr inline gen_float DotProd(const int32_t _gradient, const gen_float _x, const gen_float _y) noexcept {
		return gen_float(GRADIENT_X[_gradient]) * _x + gen_float(GRADIENT_Y[_gradient]) * _y;
	}

	static constexpr inline gen_float Contribution(const int32_t _gradient, const gen_float _x,
	                                               const gen_float _y) noexcept {
		gen_float t = 0.5 - _x * _x - _y * _y;

		if (t < 0.0)
			return 0.0;

		t *= t;
		return t * t * DotProd(_gradient, _x, _y);
	}

	constexpr inline int32_t Wrap(const gen_float _grad) {
		return _grad > 0.0 ? GenFloatToInt32(_grad) : GenFloatToInt32(_grad) - 1;
	}
};