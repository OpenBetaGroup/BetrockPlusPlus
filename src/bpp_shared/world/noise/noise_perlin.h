/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

// A recreation of the the Infdev 20100227-1433 Perlin noise function
#pragma once
#include "../../quantized_types.h"
#include "java_random.h"
#include "noise_generator.h"
#include "numeric_structs.h"
#include <array>
#include <cstdint>
#include <span>

/**
 * @brief A faithful reimplementation of the Infdev and Beta perlin noise generator
 * 
 */
class NoisePerlin : public NoiseGenerator {
public:
	NoisePerlin();
	NoisePerlin(Java::Random& _rand);
	double GenerateNoise(Vec2 _coord);
	double GenerateNoise(Vec3 _coord);
	void GenerateNoise(std::span<double> _noiseField, Vec3 _offset, Int3 _size, Vec3 _scale, double _amplitude);

private:
	double GenerateNoiseBase(Vec3 _pos);
	// Java Math functions that're only used by the generator

	// Precomputed tables for faster Gradient functions
	static constexpr std::array<int8_t, 16> K_GRAD3_U = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1 };
	static constexpr std::array<int8_t, 16> K_GRAD3_V = { 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 0, 2, 0, 2 };
	static constexpr std::array<int8_t, 16> K_GRAD2_U = { 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2 };
	static constexpr std::array<int8_t, 16> K_GRAD2_V = { 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1 };
	static constexpr std::array<gen_float, 16> K_SIGN_BIT0 = { 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1 };
	static constexpr std::array<gen_float, 16> K_SIGN_BIT1 = { 1, 1, -1, -1, 1, 1, -1, -1, 1, 1, -1, -1, 1, 1, -1, -1 };

	/**
	* @brief 3D Perlin noise gradient function
	* 
	* @param hash Hashed lattice value
	* @param x X of Distance Vector
	* @param y Y of Distance Vector
	* @param z Z of Distance Vector
	* @return double 
	*/
	constexpr inline gen_float Grad3d(int32_t _hash, const gen_float _x, const gen_float _y, const gen_float _z) {
		const uint32_t h = static_cast<uint32_t>(_hash) & 15u;
		const gen_float comp[3] = { _x, _y, _z };
		const gen_float u = comp[K_GRAD3_U[h]];
		const gen_float v = comp[K_GRAD3_V[h]];
		return u * K_SIGN_BIT0[h] + v * K_SIGN_BIT1[h];
	}

	/**
	* @brief 2D Perlin noise gradient function
	* 
	* @param hash Hashed lattice value
	* @param x X of Distance Vector
	* @param y Y of Distance Vector
	* @return double 
	*/
	constexpr inline gen_float Grad2d(int32_t _hash, const gen_float _x, const gen_float _y) {
		const uint32_t h = static_cast<uint32_t>(_hash) & 15u;
		const gen_float comp[3] = { _x, _y, 0.0 };
		const gen_float u = comp[K_GRAD2_U[h]];
		const gen_float v = comp[K_GRAD2_V[h]];
		return u * K_SIGN_BIT0[h] + v * K_SIGN_BIT1[h];
	}

	/**
	* @brief Perlin-noise easing function
	* 
	* @param value Input value
	* @return Eased output value 
	*/
	constexpr inline gen_float Fade(const gen_float _value) {
		return _value * _value * _value * (_value * (_value * gen_float(6.0) - gen_float(15.0)) + gen_float(10.0));
	}
};
