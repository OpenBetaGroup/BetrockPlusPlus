/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "spawner.h"
#include "entities/entity_animal.h"
#include "entities/entity_chicken.h"
#include "entities/entity_cow.h"
#include "entities/entity_creeper.h"
#include "entities/entity_mob.h"
#include "entities/entity_pig.h"
#include "entities/entity_sheep.h"
#include "entities/entity_skeleton.h"
#include "entities/entity_slime.h"
#include "entities/entity_spider.h"
#include "entities/entity_squid.h"
#include "entities/entity_wolf.h"
#include "entities/entity_zombie.h"
#include "entities/entity_zombie_pig.h"
#include "entities/entity_ghast.h"
#include "world.h"

EntitySpawner::EntitySpawner() {
	// Water
	SpawnCategory water;
	water.spawnMaterial = Material::Water();
	water.spawnListDefault = { { std::make_shared<SquidEntity>(), []() { return std::make_shared<SquidEntity>(); }, 10 } };
	water.spawnListForest = water.spawnListDefault;
	water.cap = 5;
	categories.push_back(water);

	// Passive
	SpawnCategory passive;
	passive.spawnListDefault = {
		{ std::make_shared<PigEntity>(), []() { return std::make_shared<PigEntity>(); }, 10 },
		{ std::make_shared<CowEntity>(), []() { return std::make_shared<CowEntity>(); }, 8 },
		{ std::make_shared<ChickenEntity>(), []() { return std::make_shared<ChickenEntity>(); }, 10 },
		{ std::make_shared<SheepEntity>(), []() { return std::make_shared<SheepEntity>(); }, 12 },
	};
	passive.spawnListForest = {
		{ std::make_shared<PigEntity>(), []() { return std::make_shared<PigEntity>(); }, 10 },
		{ std::make_shared<CowEntity>(), []() { return std::make_shared<CowEntity>(); }, 8 },
		{ std::make_shared<ChickenEntity>(), []() { return std::make_shared<ChickenEntity>(); }, 10 },
		{ std::make_shared<SheepEntity>(), []() { return std::make_shared<SheepEntity>(); }, 12 },
		{ std::make_shared<WolfEntity>(), []() { return std::make_shared<WolfEntity>(); }, 2, /*maxSpawnedInChunk=*/8 }
	};
	passive.cap = 15;
	categories.push_back(passive);

	// Hostile
	SpawnCategory hostile;
	hostile.spawnListDefault = {
		{ std::make_shared<ZombieEntity>(), []() { return std::make_shared<ZombieEntity>(); }, 10 },
		{ std::make_shared<CreeperEntity>(), []() { return std::make_shared<CreeperEntity>(); }, 10 },
		{ std::make_shared<SpiderEntity>(), []() { return std::make_shared<SpiderEntity>(); }, 10 },
		{ std::make_shared<SkeletonEntity>(), []() { return std::make_shared<SkeletonEntity>(); }, 10 },
		{ std::make_shared<SlimeEntity>(), []() { return std::make_shared<SlimeEntity>(); }, 10 },
	};
	hostile.spawnListForest = {
		{ std::make_shared<ZombieEntity>(), []() { return std::make_shared<ZombieEntity>(); }, 10 },
		{ std::make_shared<CreeperEntity>(), []() { return std::make_shared<CreeperEntity>(); }, 10 },
		{ std::make_shared<SpiderEntity>(), []() { return std::make_shared<SpiderEntity>(); }, 10 },
		{ std::make_shared<SkeletonEntity>(), []() { return std::make_shared<SkeletonEntity>(); }, 10 },
		{ std::make_shared<SlimeEntity>(), []() { return std::make_shared<SlimeEntity>(); }, 10 },
	};
	hostile.spawnListNether = {
		{ std::make_shared<GhastEntity>(), []() { return std::make_shared<GhastEntity>(); }, 10, /*maxSpawnedInChunk=*/1 },
		{ std::make_shared<ZombiePigEntity>(), []() { return std::make_shared<ZombiePigEntity>(); }, 10 },
	};
	hostile.cap = 70;
	categories.push_back(hostile);
}

// Picks one entry from the weighted list
static std::optional<SpawnEntry> PickWeighted(Java::Random& _rand, SpawnCategory& _category, Biome _biome) {
	int total = 0;
	auto& spawnList = _category.GetSpawnSpawnListForBiome(_biome);

	if (spawnList.size() == 0)
		return std::nullopt;

	for (auto& entry : spawnList)
		total += entry.weight;

	int roll = _rand.NextInt(total);
	for (auto& entry : spawnList) {
		roll -= entry.weight;
		if (roll < 0)
			return entry;
	}
	return std::nullopt;
}

static bool IsValidSpawnBlock(WorldManager& _world, Int3 _pos, Material& _material) {
	if (_material.type == MaterialType::Water) {
		// This check technically means squids can spawn in lava lol
		return _world.GetMaterial(_pos).isLiquid && !_world.IsBlockNormalCube({ _pos.x, _pos.y + 1, _pos.z });
	}
	return _world.IsBlockNormalCube({ _pos.x, _pos.y - 1, _pos.z }) && !_world.IsBlockNormalCube(_pos) &&
	       !_world.GetMaterial(_pos).isLiquid && !_world.IsBlockNormalCube({ _pos.x, _pos.y + 1, _pos.z });
}

int EntitySpawner::GetCategoryCount(WorldManager& _world, SpawnCategory& _category) {
	int count = 0;
	std::unordered_set<EntityType> types;
	for (auto& spawnType : _category.GetSpawnSpawnListForBiome(BIOME_FOREST)) {
		types.insert(spawnType.entityPtrShared->type);
	}
	for (auto& spawnType : _category.GetSpawnSpawnListForBiome(BIOME_HELL)) {
		types.insert(spawnType.entityPtrShared->type);
	}
	for (auto& spawnType : _category.GetSpawnSpawnListForBiome(BIOME_NONE)) {
		types.insert(spawnType.entityPtrShared->type);
	}
	for (auto& type : types)
		count += _world.entityManager.CountEntitiesOfType(type);
	return count;
}

void EntitySpawner::TrySpawnEntities(WorldManager& _world, const std::vector<ClientPosition>& _players) {
	BuildActiveSet(_world, _players);

	if (chunksToSpawnIn.empty())
		return;

	for (auto& category : this->categories) {
		int mobCap = category.cap * int(chunksToSpawnIn.size()) / 256;

		if (GetCategoryCount(_world, category) > mobCap)
			continue;

		for (auto& cpos : chunksToSpawnIn) {
			auto biome = _world.GetBiome({ cpos.x << 4, cpos.z << 4 });
			auto picked = PickWeighted(_world.rand, category, biome);

			if (!picked.has_value())
				continue;

			Int3 anchorPos = { cpos.x * CHUNK_WIDTH + _world.rand.NextInt(CHUNK_WIDTH),
			                   _world.rand.NextInt(CHUNK_HEIGHT),
			                   cpos.z * CHUNK_WIDTH + _world.rand.NextInt(CHUNK_WIDTH) };

			// The anchor must not be a solid cube, and its material must be the category's spawn material
			if (_world.IsBlockNormalCube(anchorPos) || _world.GetMaterial(anchorPos) != category.spawnMaterial)
				continue; // Discard this chunk

			const int maxCluster = picked.value().maxSpawnedInChunk;
			int spawnedThisCluster = 0;
			for (int group = 0; group < 3 && spawnedThisCluster < maxCluster; group++) {
				Int3 pos = anchorPos;

				for (int attempt = 0; attempt < 4; attempt++) {
					pos.x += _world.rand.NextInt(6) - _world.rand.NextInt(6);
					// This is always 0 but java rolls these here and NextInt consumes RNG state
					pos.y += _world.rand.NextInt(1) - _world.rand.NextInt(1);
					pos.z += _world.rand.NextInt(6) - _world.rand.NextInt(6);

					if (!IsValidSpawnBlock(_world, pos, category.spawnMaterial))
						continue;
					if (_world.entityManager.GetClosestPlayerWithin(pos, 24.0) != nullptr)
						continue;
					if (pos.Distance(_world.GetSpawnPoint(false)) < 24.0)
						continue;

					// Reused template instance
					auto& candidate = picked.value().entityPtrShared;
					candidate->world = &_world;
					candidate->entityManager = &_world.entityManager;

					Vec3 position = { pos.x + 0.5, double(pos.y), pos.z + 0.5 };
					float rotationYaw = _world.rand.NextFloat() * 360.0f;

					candidate->Teleport(position, { rotationYaw, 0.0 });
					if (!candidate->CanSpawnAt())
						continue;

					// Once we confirm a spawn we actually make the new entity
					std::shared_ptr<Entity> newMob = picked.value().factory();
					newMob->world = &_world;
					newMob->entityManager = &_world.entityManager;
					newMob->Teleport(position, { rotationYaw, 0.0 });

					_world.entityManager.AddEntity(newMob);
					spawnedThisCluster++;

					if (spawnedThisCluster >= maxCluster)
						break;
				}
			}
		}
	}
}

void EntitySpawner::BuildActiveSet(WorldManager& _world, const std::vector<ClientPosition>& _players) {
	int range = std::min(8, _world.GetSimulationDistance());
	chunksToSpawnIn.clear();

	for (auto& player : _players) {
		Int2 playerChunkPos = player.GetChunkPos();
		for (int dx = -range; dx <= range; dx++) {
			for (int dz = -range; dz <= range; dz++) {
				Int2 chunkPos = { playerChunkPos.x + dx, playerChunkPos.z + dz };
				auto* chunk = _world.GetChunkRaw(chunkPos);
				if (chunk && chunk->state.load() == ChunkState::Populated)
					chunksToSpawnIn.insert(chunkPos);
			}
		}
	}
}