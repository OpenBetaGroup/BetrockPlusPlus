/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once
#include "enums/dimensions.h"
#include "world/generator/generator.h"
#include "world/generator/overworld/biome_gen.h"

// Get the information needed by the world for biomes
struct BiomeGetter {
	virtual Biome GetBiomeAtPoint(Int2 _worldPos);
	virtual void GenerateBiomeMap(PackedArray<CHUNK_AREA, 4>& _biomeMap, std::span<double> _temperature,
	                      std::span<double> _humidity, std::span<double> _weirdness, Int2 _blockPos);

	virtual bool BiomeHasRain(Biome /*_biome*/) {
		return false;
	}

	virtual bool BiomeHasSnow(Biome /*_biome*/) {
		return false;
	}

	virtual bool BiomeHasLightning(Biome /*_biome*/) {
		return false;
	}

	// TODO: make biome generator have a base class that the overworld biome gen inherits?
	virtual std::unique_ptr<BiomeGenerator> GetNewBiomeGen() {
		return nullptr;
	}
};

// The base wrapper of each dimension
struct DimensionBase {
	Dimension id = {};
	int64_t seed = 0;
	std::unique_ptr<BiomeGetter> biomeGetter;

	// When disabled, turns off skylighting
	bool hasSky = true;

	// When disabled, turns off weather
	bool hasWeather = true;

	DimensionBase(int64_t _seed) : seed(_seed) {};
	virtual ~DimensionBase() = default;

	virtual std::unique_ptr<Generator> GetNewGenerator() {
		return nullptr;
	}

	virtual void UpdateSeed(int64_t _seed) {
		seed = _seed;
	}
};