/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once
#include "world/dimension/dimension.h"
#include "world/generator/overworld/biome_gen.h"
#include "world/generator/overworld/chunk_gen.h"

// Get the information needed by the world for biomes
struct BiomeGetterOverworld : BiomeGetter {
	int64_t seed = 0;
	BiomeGenerator overworldBiomeGen;

	BiomeGetterOverworld(int64_t _seed) : seed(_seed), overworldBiomeGen(_seed) {};

	Biome GetBiomeAtPoint(Int2 _worldPos) {
		return overworldBiomeGen.GetBiomeAtPoint(_worldPos);
	}
	void GenerateBiomeMap(PackedArray<CHUNK_AREA, 4>& _biomeMap, std::span<double> _temperature,
	                      std::span<double> _humidity, std::span<double> _weirdness, Int2 _blockPos) override {
		overworldBiomeGen.GenerateBiomeMap(_biomeMap, _temperature, _humidity, _weirdness, _blockPos);
	}

	bool BiomeHasRain(Biome _biome) override {
		return overworldBiomeGen.BiomeHasRain(_biome);
	}

	bool BiomeHasSnow(Biome _biome) override {
		return overworldBiomeGen.BiomeHasSnow(_biome);
	}

	bool BiomeHasLightning(Biome _biome) override {
		return overworldBiomeGen.BiomeHasLightning(_biome);
	}

	std::unique_ptr<BiomeGenerator> GetNewBiomeGen() override {
		return std::make_unique<BiomeGenerator>(seed);
	}
};

struct DimensionOverworld : DimensionBase {
	DimensionOverworld(int64_t _seed) : DimensionBase(_seed) {
		this->biomeGetter = std::make_unique<BiomeGetterOverworld>(_seed);
		id = Dimension::Overworld;
	}

	std::unique_ptr<Generator> GetNewGenerator() override {
		return std::make_unique<OverworldGenerator>(this->seed);
	}

	void UpdateSeed(int64_t _seed) override {
		DimensionBase::UpdateSeed(_seed);
		this->biomeGetter = std::make_unique<BiomeGetterOverworld>(_seed);
	}
};