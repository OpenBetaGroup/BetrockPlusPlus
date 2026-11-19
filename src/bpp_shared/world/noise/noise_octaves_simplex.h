/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#pragma once

#include "java_random.h"
#include "noise_simplex.h"
#include "numeric_structs.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

class NoiseOctavesSimplex {
public:
	NoiseOctavesSimplex() {}
	NoiseOctavesSimplex(int32_t _octaves);
	NoiseOctavesSimplex(Java::Random& _rand, int32_t _octaves);

	// func_4112_a
	void GenerateOctaves(std::span<double> _noiseField, Vec2 _offset, Int32_2 _size, Vec2 _scale, double _lacunarity);
	void GenerateOctaves(std::span<double> _noiseField, Int32_2 _offset, Int32_2 _size, Vec2 _scale, double _lacunarity);
	// func_4111_a
	void GenerateOctaves(std::span<double> _noiseField, Vec2 _offset, Int32_2 _size, Vec2 _scale, double _lacunarity,
	                     double _persistence);

private:
	int32_t octaves = 0;
	std::vector<NoiseSimplex> generatorCollection;
};

inline NoiseOctavesSimplex::NoiseOctavesSimplex(int32_t _poctaves) : octaves(_poctaves) {
	generatorCollection.reserve(size_t(octaves));
	for (size_t i = 0; i < size_t(octaves); ++i)
		generatorCollection.push_back(NoiseSimplex());
}

inline NoiseOctavesSimplex::NoiseOctavesSimplex(Java::Random& _rand, int32_t _poctaves) : octaves(_poctaves) {
	generatorCollection.reserve(size_t(octaves));
	for (size_t i = 0; i < size_t(octaves); ++i)
		generatorCollection.push_back(NoiseSimplex(_rand));
}

inline void NoiseOctavesSimplex::GenerateOctaves(std::span<double> _noiseField, Int32_2 _offset, Int32_2 _size,
                                                 Vec2 _scale, double _lacunarity) {
	this->GenerateOctaves(_noiseField, Vec2{ double(_offset.x), double(_offset.y) }, _size, _scale, _lacunarity);
}

inline void NoiseOctavesSimplex::GenerateOctaves(std::span<double> _noiseField, Vec2 _offset, Int32_2 _size,
                                                 Vec2 _scale, double _lacunarity) {
	this->GenerateOctaves(_noiseField, _offset, _size, _scale, _lacunarity, 0.5);
}

inline void NoiseOctavesSimplex::GenerateOctaves(std::span<double> _noiseField, Vec2 _offset, Int32_2 _size,
                                                 Vec2 _scale, double _lacunarity, double _persistence) {
	_scale.x /= 1.5;
	_scale.y /= 1.5;
	// Int32_2: y and z are the same union member — do NOT multiply by .z (that would be x*y*y).
	const size_t count = size_t(_size.x) * size_t(_size.y);
	if (_noiseField.size() < count)
		return;

	std::fill_n(_noiseField.data(), count, 0.0);

	double frequency = 1.0;
	double amplitude = 1.0;
	for (size_t octave = 0; octave < size_t(octaves); ++octave) {
		generatorCollection[octave].GenerateNoise(_noiseField.first(count), _offset, _size, _scale * amplitude,
		                                          0.55 / frequency);
		amplitude *= _lacunarity;
		frequency *= _persistence;
	}
}