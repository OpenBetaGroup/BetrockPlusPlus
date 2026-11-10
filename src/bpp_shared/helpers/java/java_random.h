/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 * Based on code by Oracle/OpenJDK (1995-2024)
*/

// A reimplementation of the random function that Java provides
// https://docs.oracle.com/javase/8/docs/api/java/util/Random.html

// For more info, cross-reference with JDK source
// https://github.com/openjdk/jdk8u-dev/blob/master/jdk/src/share/classes/java/util/Random.java

#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>

/**
 * @brief A faithful reimplementation of the Java pseudorandom number generator
 * 
 */
namespace Java {
class Random {
private:
	static constexpr uint64_t M_MULTIPLIER = 0x5DEECE66DULL;
	static constexpr uint64_t M_ADDEND = 0xBULL;
	static constexpr uint64_t M_MASK = (1ULL << 48) - 1;
	double nextNextGaussian;
	bool haveNextNextGaussian = false;

	uint64_t seed;

	/**
		* @brief Performs a new iteration of the PRNG
		* 
		* @return Pseudorandom 32-bit integer value
		*/
	int32_t Next(const int32_t _bits) noexcept {
		seed = (seed * M_MULTIPLIER + M_ADDEND) & M_MASK;
		return static_cast<int32_t>(seed >> (48 - _bits));
	}

public:
	/**
		* @brief Construct a new Java Random object
		* 
		* @param initialSeed The initial seed value (defaults to current time)
		*/
	Random(const int64_t _initialSeed) {
		SetSeed(_initialSeed);
		haveNextNextGaussian = false;
	}

	/**
		* @brief Construct a new Java Random object
		* 
		*/
	Random() {
		// Default seed: current time
		SetSeed(static_cast<int64_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
	}

	/**
		* @brief Set the Seed value that'll be used for all subsequently generated values
		* 
		* @param s Seed value
		*/
	void SetSeed(const int64_t _s) noexcept {
		seed = (static_cast<uint64_t>(_s) ^ M_MULTIPLIER) & M_MASK;
	}

	/**
		* @brief Returns the next int32_t (32-bit integer)
		* 
		* @return Pseudorandom 32-bit integer value
		*/
	int32_t NextInt() noexcept {
		return Next(32);
	}

	/**
		* @brief Returns the next bound int32_t (32-bit integer)
		* 
		* @return Pseudorandom 32-bit integer value
		*/
	int32_t NextInt(const int32_t _bound) {
		if (_bound <= 0)
			throw std::invalid_argument("bound must be positive");

		if ((_bound & -_bound) == _bound) { // power of two
			return int32_t((_bound * int64_t(Next(31))) >> 31);
		}

		int32_t bits, val;
		do {
			bits = Next(31);
			val = bits % _bound;
		} while (int32_t(uint32_t(bits) - uint32_t(val) + uint32_t(_bound - 1)) < 0);
		return val;
	}

	/**
		* @brief Returns the next long (64-bit integer)
		* 
		* @return Pseudorandom 64-bit long value
		*/
	int64_t NextLong() noexcept {
		return (int64_t(Next(32)) << 32) + Next(32);
	}

	/**
		* @brief Returns the next pseudorandom double
		* 
		* @return Pseudorandom double value between 0.0 (inclusive) and 1.0 (exclusive)
		*/
	double NextDouble() noexcept {
		return double((int64_t(Next(26)) << 27) + Next(27)) / double(1LL << 53);
	}

	/**
		* @brief Returns the next pseudorandom boolean
		* 
		* @return Pseudorandom boolean
		*/
	bool NextBoolean() noexcept {
		return Next(1) != 0;
	}

	/**
		* @brief Returns the next pseudorandom float
		* 
		* @return Pseudorandom float value between 0.0 (inclusive) and 1.0 (exclusive)
		*/
	float NextFloat() noexcept {
		return float(Next(24)) / float(1 << 24);
	}

	/**
		* @brief Returns the next pseudorandom gaussian
		* 
		* @return Pseudorandom gaussian with a mean of 0.0, and a standard deviation of 1.0
		*/
	double NextGaussian() noexcept {
		// See Knuth, ACP, Section 3.4.1 Algorithm C.
		if (haveNextNextGaussian) {
			haveNextNextGaussian = false;
			return nextNextGaussian;
		}
		double v1, v2, s;
		do {
			v1 = 2 * NextDouble() - 1; // between -1 and 1
			v2 = 2 * NextDouble() - 1; // between -1 and 1
			s = v1 * v1 + v2 * v2;
		} while (s >= 1 || s == 0);
		double multiplier = std::sqrt(-2 * std::log(s) / s);
		nextNextGaussian = v2 * multiplier;
		haveNextNextGaussian = true;
		return v1 * multiplier;
	}
};
} // namespace Java