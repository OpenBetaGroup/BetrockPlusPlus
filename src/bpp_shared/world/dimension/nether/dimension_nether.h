/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once
#include "world/dimension/dimension.h"
#include "world/generator/nether/chunk_gen.h"

// Get the information needed by the world for biomes
struct BiomeGetterNether : BiomeGetter {
	Biome GetBiomeAtPoint(Int2 _worldPos) {
		return Biome::BIOME_HELL;
	}
	void GenerateBiomeMap(PackedArray<CHUNK_AREA, 4>& _biomeMap, std::span<double> _temperature,
	                      std::span<double> _humidity, std::span<double> _weirdness, Int2 _blockPos) override;
	bool BiomeHasRain(Biome _biome) override {
		return false;
	}

	bool BiomeHasSnow(Biome _biome) override {
		return false;
	}

	bool BiomeHasLightning(Biome _biome) override {
		return false;
	}
};

struct DimensionNether : DimensionBase {
	DimensionNether(int64_t _seed) : DimensionBase(_seed) {
		this->biomeGetter = std::make_unique<BiomeGetterNether>();
		id = Dimension::Nether;
		hasSky = false;
		hasWeather = false;
	}

	std::unique_ptr<Generator> GetNewGenerator() override {
		return std::make_unique<NetherGenerator>(this->seed);
	}
};