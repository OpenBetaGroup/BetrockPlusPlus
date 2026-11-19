/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "noise_perlin.h"
#include "java_math.h"
#include <cstddef>

NoisePerlin::NoisePerlin() {
	Java::Random rand = Java::Random();
	InitPermTable(rand);
}

/**
 * @brief Construct a new Noise Perlin object
 * 
 * @param rand The random number generator that should be used
 */
NoisePerlin::NoisePerlin(Java::Random& _rand) {
	InitPermTable(_rand);
}

/**
 * @brief This is a rather standard implementation of "Improved Perlin Noise",
 *        as described by Ken Perlin in 2002
 *        This version is mainly used by the Infdev generator
 *        but Beta still implements and uses it for some things,
 *        namely the nether
 * 
 * @param pos Coordinate at which to sample the noise
 * @return Noise value
 */
double NoisePerlin::GenerateNoiseBase(Vec3 _pos) {
	_pos.x += coordinate.x;
	_pos.y += coordinate.y;
	_pos.z += coordinate.z;
	// The farlands are caused by this getting cast to a 32-Bit Integer.
	// Change these int32_t to int64_t to fix the farlands in Infdev
	int32_t xInt = Java::DoubleToInt32(_pos.x);
	int32_t yInt = Java::DoubleToInt32(_pos.y);
	int32_t zInt = Java::DoubleToInt32(_pos.z);
	if (_pos.x < gen_float(xInt))
		--xInt;
	if (_pos.y < gen_float(yInt))
		--yInt;
	if (_pos.z < gen_float(zInt))
		--zInt;

	int32_t xIndex = xInt & 255;
	int32_t yIndex = yInt & 255;
	int32_t zIndex = zInt & 255;

	_pos.x -= xInt;
	_pos.y -= yInt;
	_pos.z -= zInt;
	gen_float w = Fade(_pos.x);
	gen_float v = Fade(_pos.y);
	gen_float u = Fade(_pos.z);
	int32_t permXY = permutations[xIndex] + yIndex;
	int32_t permXYZ = permutations[permXY] + zIndex;
	// Some of the following code is weird,
	// probably because it got optimized by Java to use
	// fewer variables or Notch did this to be efficient
	permXY = permutations[permXY + 1] + zIndex;
	xIndex = permutations[xIndex + 1] + yIndex;
	yIndex = permutations[xIndex] + zIndex;
	xIndex = permutations[xIndex + 1] + zIndex;
	return double(Lerp(u,
	                   Lerp(v,
	                        Lerp(w, Grad3d(permutations[permXYZ], _pos.x, _pos.y, _pos.z),
	                             Grad3d(permutations[yIndex], _pos.x - 1.0, _pos.y, _pos.z)),
	                        Lerp(w, Grad3d(permutations[permXY], _pos.x, _pos.y - 1.0, _pos.z),
	                             Grad3d(permutations[xIndex], _pos.x - 1.0, _pos.y - 1.0, _pos.z))),
	                   Lerp(v,
	                        Lerp(w, Grad3d(permutations[permXYZ + 1], _pos.x, _pos.y, _pos.z - 1.0),
	                             Grad3d(permutations[yIndex + 1], _pos.x - 1.0, _pos.y, _pos.z - 1.0)),
	                        Lerp(w, Grad3d(permutations[permXY + 1], _pos.x, _pos.y - 1.0, _pos.z - 1.0),
	                             Grad3d(permutations[xIndex + 1], _pos.x - 1.0, _pos.y - 1.0, _pos.z - 1.0)))));
}

double NoisePerlin::GenerateNoise(Vec2 _coord) {
	return GenerateNoiseBase(Vec3{ _coord.x, _coord.y, 0.0 });
}

double NoisePerlin::GenerateNoise(Vec3 _coord) {
	return GenerateNoiseBase(_coord);
}

/**
 * @brief The main noise generator employed by the Beta 1.7.3 world generator
 * 
 * @param noiseField the buffer the noise will be written to (must hold size.x*y*z samples)
 * @param offset The positional offset within the perlin noise that'll be rendered
 * @param size The size of the volume that'll be saved the noise field
 * @param scale The scale of the perlin noise equation
 * @param amplitude The amplitude multiplier of the perlin noise function
 */
void NoisePerlin::GenerateNoise(std::span<double> _noiseField, Vec3 _offset, Int3 _size, Vec3 _scale,
                                double _amplitude) {
	const size_t count = size_t(_size.x) * size_t(_size.y) * size_t(_size.z);
	if (_noiseField.size() < count)
		return;

	double* out = _noiseField.data();
	const gen_float invAmp = 1.0 / _amplitude;

	if (_size.y == 1) {
		for (int32_t x = 0; x < _size.x; ++x) {
			gen_float fx = (_offset.x + x) * _scale.x + coordinate.x;
			int32_t ix = GenFloatToInt32(fx);
			if (fx < ix)
				--ix;
			int32_t px = ix & 255;
			fx -= ix;
			gen_float u = Fade(fx);

			for (int32_t z = 0; z < _size.z; ++z) {
				gen_float fz = (_offset.z + z) * _scale.z + coordinate.z;
				int32_t iz = GenFloatToInt32(fz);
				if (fz < iz)
					--iz;
				int32_t pz = iz & 255;
				fz -= iz;
				gen_float w = Fade(fz);

				int32_t a = permutations[px] + 0;
				int32_t aa = permutations[a] + pz;
				int32_t b = permutations[px + 1] + 0;
				int32_t ba = permutations[b] + pz;

				gen_float x1 = Lerp(u, Grad2d(permutations[aa], fx, fz), Grad3d(permutations[ba], fx - 1.0, 0.0, fz));

				gen_float x2 = Lerp(u, Grad3d(permutations[aa + 1], fx, 0.0, fz - 1.0),
				                    Grad3d(permutations[ba + 1], fx - 1.0, 0.0, fz - 1.0));

				*out++ += double(Lerp(w, x1, x2) * invAmp);
			}
		}
	} else {
		int32_t lastPermY = -1;

		gen_float lerpAX = 0.0, lerpBX = 0.0;
		gen_float lerpAY = 0.0, lerpBY = 0.0;

		for (int32_t x = 0; x < _size.x; ++x) {
			gen_float fx = (_offset.x + x) * _scale.x + coordinate.x;
			int32_t ix = GenFloatToInt32(fx);
			if (fx < ix)
				--ix;
			int32_t px = ix & 255;
			fx -= ix;
			gen_float u = Fade(fx);

			for (int32_t z = 0; z < _size.z; ++z) {
				gen_float fz = (_offset.z + z) * _scale.z + coordinate.z;
				int32_t iz = GenFloatToInt32(fz);
				if (fz < iz)
					--iz;
				int32_t pz = iz & 255;
				fz -= iz;
				gen_float w = Fade(fz);

				for (int32_t y = 0; y < _size.y; ++y) {
					gen_float fy = (_offset.y + y) * _scale.y + coordinate.y;
					int32_t iy = GenFloatToInt32(fy);
					if (fy < iy)
						--iy;
					int32_t py = iy & 255;
					fy -= iy;
					gen_float v = Fade(fy);

					// Beta caches corner grads when the lattice Y index is unchanged.
					// Keep that reuse (including stale fractional fy) for terrain fidelity.
					if (y == 0 || py != lastPermY) {
						lastPermY = py;

						int32_t a = permutations[px] + py;
						int32_t aa = permutations[a] + pz;
						int32_t ab = permutations[a + 1] + pz;
						int32_t b = permutations[px + 1] + py;
						int32_t ba = permutations[b] + pz;
						int32_t bb = permutations[b + 1] + pz;

						lerpAX = Lerp(u, Grad3d(permutations[aa], fx, fy, fz),
						              Grad3d(permutations[ba], fx - 1.0, fy, fz));

						lerpBX = Lerp(u, Grad3d(permutations[ab], fx, fy - 1.0, fz),
						              Grad3d(permutations[bb], fx - 1.0, fy - 1.0, fz));

						lerpAY = Lerp(u, Grad3d(permutations[aa + 1], fx, fy, fz - 1.0),
						              Grad3d(permutations[ba + 1], fx - 1.0, fy, fz - 1.0));

						lerpBY = Lerp(u, Grad3d(permutations[ab + 1], fx, fy - 1.0, fz - 1.0),
						              Grad3d(permutations[bb + 1], fx - 1.0, fy - 1.0, fz - 1.0));
					}

					gen_float i1 = Lerp(v, lerpAX, lerpBX);
					gen_float i2 = Lerp(v, lerpAY, lerpBY);
					*out++ += double(Lerp(w, i1, i2) * invAmp);
				}
			}
		}
	}
}
