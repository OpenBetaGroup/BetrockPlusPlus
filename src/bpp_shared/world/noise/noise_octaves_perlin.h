/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#pragma once

#include "java_random.h"
#include "noise_perlin.h"
#include "numeric_structs.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

class NoiseOctavesPerlin {
public:
	NoiseOctavesPerlin() {}
	NoiseOctavesPerlin(int32_t _octaves);
	NoiseOctavesPerlin(Java::Random& _rand, int32_t _octaves);

	// func_647_a
	double GenerateOctaves(Vec2 _offset);
	// generateNoiseOctaves
	void GenerateOctaves(std::span<double> _noiseField, Vec3 _coordinate, Int32_3 _size, Vec3 _pScale);
	// func_4103_a
	void GenerateOctaves(std::span<double> _noiseField, Int32_2 _offset, Int32_2 _size, Vec2 _scale, double /*_unused*/);

private:
	int32_t octaves = 0;
	std::vector<NoisePerlin> generatorCollection;
};

inline NoiseOctavesPerlin::NoiseOctavesPerlin(int32_t _poctaves) : octaves(_poctaves) {
	generatorCollection.reserve(size_t(octaves));
	for (size_t i = 0; i < size_t(octaves); ++i)
		generatorCollection.push_back(NoisePerlin());
}

inline NoiseOctavesPerlin::NoiseOctavesPerlin(Java::Random& _rand, int32_t _poctaves) : octaves(_poctaves) {
	generatorCollection.reserve(size_t(octaves));
	for (size_t i = 0; i < size_t(octaves); ++i)
		generatorCollection.push_back(NoisePerlin(_rand));
}

inline double NoiseOctavesPerlin::GenerateOctaves(Vec2 _offset) {
	double value = 0.0;
	double scale = 1.0;
	for (size_t i = 0; i < size_t(octaves); ++i) {
		value += generatorCollection[i].GenerateNoise(_offset * scale) / scale;
		scale /= 2.0;
	}
	return value;
}

inline void NoiseOctavesPerlin::GenerateOctaves(std::span<double> _noiseField, Vec3 _coordinate, Int32_3 _size,
                                                Vec3 _pScale) {
	const size_t count = size_t(_size.x) * size_t(_size.y) * size_t(_size.z);
	if (_noiseField.size() < count)
		return;

	std::fill_n(_noiseField.data(), count, 0.0);

	double multiplier = 1.0;
	for (size_t octave = 0; octave < size_t(octaves); ++octave) {
		generatorCollection[octave].GenerateNoise(_noiseField.first(count), _coordinate, _size, _pScale * multiplier,
		                                          multiplier);
		multiplier /= 2.0;
	}
}

inline void NoiseOctavesPerlin::GenerateOctaves(std::span<double> _noiseField, Int32_2 _offset, Int32_2 _size,
                                                Vec2 _scale, double /*_unused*/) {
	this->GenerateOctaves(_noiseField, Vec3{ double(_offset.x), 10.0, double(_offset.z) },
	                      Int32_3{ _size.x, 1, _size.z }, Vec3{ _scale.x, 1.0, _scale.z });
}
