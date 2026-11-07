/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "tile_entity_mob_spawner.h"
#include "entities/entity_mob.h"
#include "entities/entity_pig.h"
#include "entities/entity_skeleton.h"
#include "entities/entity_spider.h"
#include "entities/entity_zombie.h"

void TileEntityMobSpawner::Deserialize(const Tag& _tag) {
	if (_tag.Has("EntityId"))
		this->entityId = _tag.Get("EntityId").GetString();
	if (_tag.Has("Delay"))
		this->delay = _tag.Get("Delay").GetShort();
}

bool TileEntityMobSpawner::PlayerInRange(WorldManager& _world) {
	return _world.entityManager.GetClosestPlayerWithin(Vec3{ position.x + 0.5, position.y + 0.5, position.z + 0.5 },
	                                                   16) != nullptr;
}

void TileEntityMobSpawner::Tick(WorldManager& _world) {
	if (!PlayerInRange(_world))
		return;

	auto updateDelay = [](Java::Random& _rand) -> int {
		return 200 + _rand.NextInt(600);
	};

	// Why not <= 0? Because notch wrote this as "delay == -1"..
	// For some reason
	if (this->delay < 0)
		this->delay = updateDelay(_world.rand);
	else {
		this->delay--;
		return;
	}

	for (size_t attempt = 0; attempt < 4; attempt++) {
		// TODO: make a helper in the entity manager for this
		std::shared_ptr<Entity> myEntity;
		EntityType checkType = EntityType::NONE;
		if (this->entityId == "Skeleton") {
			myEntity = std::make_shared<SkeletonEntity>();
			checkType = EntityType::SKELETON;
		} else if (this->entityId == "Zombie") {
			myEntity = std::make_shared<ZombieEntity>();
			checkType = EntityType::ZOMBIE;
		} else if (this->entityId == "Spider") {
			myEntity = std::make_shared<SpiderEntity>();
			checkType = EntityType::SPIDER;
		} else {
			myEntity = std::make_shared<PigEntity>();
			checkType = EntityType::PIG;
		}

		if (!myEntity)
			return;

		AABB searchBox = {
			double(position.x),       double(position.y),       double(position.z),
			double(position.x) + 1.0, double(position.y) + 1.0, double(position.z) + 1.0,
		};
		auto numEntities =
		    _world.entityManager.GetEntitiesWithinAabbOfType(searchBox.Expand(8.0, 4.0, 8.0), checkType).size();
		if (numEntities >= 6) {
			this->delay = updateDelay(_world.rand);
			return;
		}

		Vec3 spawn = {};
		spawn.x = double(position.x) + (_world.rand.NextDouble() - _world.rand.NextDouble()) * 4.0;
		spawn.y = double(position.y) + _world.rand.NextInt(3) - 1;
		spawn.z = double(position.z) + (_world.rand.NextDouble() - _world.rand.NextDouble()) * 4.0;

		auto mobEntity = dynamic_cast<MobEntity*>(myEntity.get());

		mobEntity->world = &_world;
		mobEntity->entityManager = &_world.entityManager;
		mobEntity->Teleport(spawn, { _world.rand.NextFloat() * 360.0f, 0 });

		// Maybe not vanilla accurate?
		// This seems to improve spawn rates, though
		mobEntity->rand.SetSeed(_world.rand.NextLong());
		if (mobEntity->CanSpawnAt()) {
			_world.entityManager.AddEntity(std::move(myEntity));
			this->delay = updateDelay(_world.rand);
		}
	}
}

Tag TileEntityMobSpawner::Serialize() {
	auto root = TileEntity::Serialize();

	auto vEntityId = Tag{ .type = TAG_STRING, .name = "EntityId", .longValue = 0, .stringValue = entityId };
	auto vDelay = Tag{ .type = TAG_SHORT, .name = "Delay", .shortValue = delay };

	root.compound["EntityId"] = vEntityId;
	root.compound["Delay"] = vDelay;

	return root;
}