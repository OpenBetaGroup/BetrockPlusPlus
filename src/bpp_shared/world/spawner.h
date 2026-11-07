/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once
#include "numeric_structs.h"
#include <functional>
#include <memory>
#include <unordered_set>
#include <vector>

class WorldManager;
struct Chunk;
struct ClientPosition;
struct MobEntity;

struct SpawnEntry {
	std::shared_ptr<MobEntity> entityPtrShared;
	std::function<std::shared_ptr<Entity>()> factory;
	int weight;
	int maxSpawnedInChunk = 4;
};

struct SpawnCategory {
	std::vector<SpawnEntry>& GetSpawnSpawnListForBiome(const Biome _biome) {
		switch (_biome) {
		case BIOME_FOREST:
		case BIOME_TAIGA:
			return spawnListForest;
		case BIOME_HELL:
			return spawnListNether;
		default:
			return spawnListDefault;
		}
	}
	std::vector<SpawnEntry> spawnListDefault = {};
	std::vector<SpawnEntry> spawnListForest = {};
	std::vector<SpawnEntry> spawnListNether = {};
	int cap = 15;
	Material spawnMaterial = Material::Air();
};

// For spawning entities in the world like mobs and animals
struct EntitySpawner {
	std::unordered_set<Int32_2> chunksToSpawnIn;
	std::vector<SpawnCategory> categories;

	EntitySpawner();

	void TrySpawnEntities(WorldManager& _world, const std::vector<ClientPosition>& _players);
	void BuildActiveSet(WorldManager& _world, const std::vector<ClientPosition>& _players);
	int GetCategoryCount(WorldManager& _world, SpawnCategory& _category);
};