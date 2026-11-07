/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#include "entity_manager.h"
#include "entity_arrow.h"
#include "entity_boat.h"
#include "entity_chicken.h"
#include "entity_cow.h"
#include "entity_creeper.h"
#include "entity_egg.h"
#include "entity_falling_block.h"
#include "entity_item.h"
#include "entity_minecart.h"
#include "entity_painting.h"
#include "entity_pig.h"
#include "entity_sheep.h"
#include "entity_skeleton.h"
#include "entity_slime.h"
#include "entity_snowball.h"
#include "entity_spider.h"
#include "entity_tnt.h"
#include "entity_zombie.h"
#include "entity_squid.h"
#include "entity_wolf.h"
#include "entity_zombie_pig.h"
#include "entity_player.h"
#include "entity_ghast.h"
#include "gamerules.h"
#include "world.h"
#include "username.h"

std::shared_ptr<PlayerEntity> EntityManager::GetPlayerWithName(std::string _name) {
	// I hate this
	for (const auto& playerWeak : players) {
		if (auto playerEntity = playerWeak.lock()) {
			if (auto player = std::dynamic_pointer_cast<PlayerEntity>(playerEntity)) {
				if (UsernameEquals(player->username, _name)) {
					return player;
				}
			}
		}
	}
	return nullptr;
}

void EntityManager::AlertWolves(PlayerEntity& _player, Entity& _target, bool _ignoreSitting) {
	if (_target.type == EntityType::CREEPER || _target.type == EntityType::GHAST)
		return;

	auto wolf = dynamic_cast<WolfEntity*>(&_target);
	if (wolf) {
		if (wolf->IsTamed() && NormalizeUsername(_player.username) == NormalizeUsername(wolf->ownerUsername))
			return;
	}

	auto searchCollider = AABB{ _player.position.x,       _player.position.y,       _player.position.z,
		                        _player.position.x + 1.0, _player.position.y + 1.0, _player.position.z + 1.0 };
	searchCollider = searchCollider.Expand(16.0, 4.0, 16.0);

	EntityType wolfType = EntityType::WOLF;
	auto candidates = GetEntitiesWithinAabbOfType(searchCollider, wolfType);

	for (auto& candidate : candidates) {
		auto wolfCandidate = dynamic_cast<WolfEntity*>(candidate);
		if (!wolfCandidate)
			continue;
		if (!wolfCandidate->IsTamed())
			continue;
		if (wolfCandidate->attackTarget.lock())
			continue;
		if (NormalizeUsername(_player.username) != NormalizeUsername(wolfCandidate->ownerUsername))
			continue;
		if (_ignoreSitting && wolfCandidate->isSitting)
			continue;
		wolfCandidate->SetWolfSitting(false);
		wolfCandidate->attackTarget = GetEntityByIdShared(_target.id);
	}
}

void EntityManager::RemoveEntity(EntityId _id) {
	// Find the entity for this ID
	auto it = std::find_if(entities.begin(), entities.end(),
	                       [_id](const std::shared_ptr<Entity>& _e) { return _e->id == _id; });
	if (it == entities.end()) {
		return; // Not found, nothing to do
	}

	std::shared_ptr<Entity> entity = *it;

	// Remove player
	if (entity->type == EntityType::PLAYER)
		std::erase_if(players, [&](const std::weak_ptr<Entity>& _w) {
			auto p = _w.lock();
			return !p || p == entity;
		});

	// Remove from its bucket
	Int2 cpos{ entity->bucketPos.x, entity->bucketPos.y };
	auto containerIt = entityContainers.find(cpos);
	if (containerIt != entityContainers.end()) {
		auto& bucket = containerIt->second.buckets[entity->bucketPos.z];
		bucket.entities.erase(std::remove_if(bucket.entities.begin(), bucket.entities.end(),
		                                     [&entity](const Entity* _entity) { return _entity == entity.get(); }),
		                      bucket.entities.end());
		bucket.worldCollidableEntities.erase(
		    std::remove_if(bucket.worldCollidableEntities.begin(), bucket.worldCollidableEntities.end(),
		                   [&entity](const Entity* _entity) { return _entity == entity.get(); }),
		    bucket.worldCollidableEntities.end());
		PruneEmptyContainer(cpos);
	}

	// Remove from the master list
	entities.erase(it);

	// Unbind ourselves
	entity->entityManager = nullptr;

	// Mark the chunk we died in as dirty
	if (auto chunk = world->GetChunkRaw({ entity->bucketPos.x, entity->bucketPos.y }))
		chunk->isModified = true;

	// Set as dead for cleanup
	entity->isDead = true;
	if (onEntityDespawn)
		onEntityDespawn(entity);
}

void EntityManager::AddEntity(std::shared_ptr<Entity> _entity, EntityId _forceEntityId) {
	if (!world) {
		Logger::Error << "Attempted to add an entity before EntityManager was bound to a world!\n";
		return;
	}
	// TODO: Run this on gamerule change too?
	if (IsAnimal(_entity->type) && !Gamerule::Get<bool>(Gamerule::Id::SpawnMobs))
		return;
	if (IsMonster(_entity->type) && !Gamerule::Get<bool>(Gamerule::Id::SpawnMonsters))
		return;
	_entity->id = _forceEntityId == -1 ? GetNextEntityId()
	                                   : _forceEntityId; // Assign an ID if we weren't forced to use one
	_entity->world = world; // Bind the world pointer so the entity can interact with the world
	_entity->entityManager = this;
	_entity->dim = world->GetDimension();

	// Register the entity into its initial bucket
	_entity->bucketPos = ComputeBucketPos(_entity->position);
	auto& container = entityContainers[{ _entity->bucketPos.x, _entity->bucketPos.y }];
	container.buckets[_entity->bucketPos.z].entities.push_back(_entity.get());
	if (_entity->actsAsWorldCollider)
		container.buckets[_entity->bucketPos.z].worldCollidableEntities.push_back(_entity.get());

	// If we are a player register for easy lookup
	if (_entity->type == EntityType::PLAYER) {
		std::weak_ptr<Entity> weakEntity = _entity;
		players.push_back(weakEntity);
	}

	entities.push_back(std::move(_entity));
	if (onEntitySpawn)
		onEntitySpawn(entities.back());
}

void EntityManager::TickEntityAndPassenger(const std::shared_ptr<Entity>& _entity, TickProfiler* _profiler) {
	if (_profiler) {
		auto start = std::chrono::steady_clock::now();
		_entity->Tick();
		_profiler->RecordEntity(_entity->dim, _entity->id, _entity->type, std::chrono::steady_clock::now() - start);
	} else {
		_entity->Tick();
	}

	// Check to see if this entity went into another container or bucket
	Int3 newBucketPos = ComputeBucketPos(_entity->position);
	Entity* raw = _entity.get();

	if (newBucketPos != _entity->bucketPos) {
		Int2 oldCpos{ _entity->bucketPos.x, _entity->bucketPos.y };
		// Remove from the old bucket and mark the old chunk as modified
		if (auto chunk = world->GetChunkRaw(oldCpos))
			chunk->isModified = true;
		auto& oldContainer = entityContainers[oldCpos];
		auto& b = oldContainer.buckets[_entity->bucketPos.z];
		b.entities.erase(std::remove_if(b.entities.begin(), b.entities.end(),
		                                [&_entity](Entity* _ptr) { return _ptr == _entity.get(); }),
		                 b.entities.end());
		b.worldCollidableEntities.erase(std::remove_if(b.worldCollidableEntities.begin(),
		                                               b.worldCollidableEntities.end(),
		                                               [&_entity](Entity* _ptr) { return _ptr == _entity.get(); }),
		                                b.worldCollidableEntities.end());
		PruneEmptyContainer(oldCpos);

		// Put in the new bucket
		auto& newContainer = entityContainers[{ newBucketPos.x, newBucketPos.y }];
		auto& newB = newContainer.buckets[newBucketPos.z];
		newB.entities.push_back(raw);
		if (_entity->actsAsWorldCollider)
			newB.worldCollidableEntities.push_back(raw);
		_entity->bucketPos = newBucketPos;
	}

	// If something is riding us, tick it
	if (auto lockPassenger = _entity->passenger.lock()) {
		auto passengersVehicle = lockPassenger->vehicle.lock();
		if (!lockPassenger->isDead && passengersVehicle.get() == raw) {
			TickEntityAndPassenger(lockPassenger, _profiler);
		} else {
			_entity->passenger.reset();
		}
	}
}

void EntityManager::Tick(TickProfiler* _profiler) {
	// Snapshot weak refs so mid-tick spawn/despawn cannot invalidate iteration,
	// without the full shared_ptr vector copy every tick.
	std::vector<std::weak_ptr<Entity>> toTick;
	toTick.reserve(entities.size());
	for (const auto& entity : entities)
		toTick.emplace_back(entity);

	for (const auto& weak : toTick) {
		std::shared_ptr<Entity> entity = weak.lock();
		if (!entity || entity->isDead)
			continue;

		// Skip entities that are riding something
		if (auto lockVehicle = entity->vehicle.lock()) {
			if (!lockVehicle->isDead && lockVehicle->passenger.lock().get() == entity.get())
				continue;
			entity->vehicle.reset();
		}

		TickEntityAndPassenger(entity, _profiler);
	}

	// Remove dead entities after ticking so iteration stays stable
	for (size_t i = 0; i < entities.size();) {
		if (entities[i]->isDead)
			RemoveEntity(entities[i]->id);
		else
			++i;
	}

	// Clear players that have expired
	for (auto it = players.begin(); it != players.end();) {
		if (it->expired()) {
			it = players.erase(it);
		} else {
			++it;
		}
	}
}

std::vector<Entity*> EntityManager::GetEntitiesWithinAabbExcluding(const AABB& _box, const EntityId _entityId) {
	// Get all entities within an AABB excluding this entity id
	auto entitiesInAABB = GetEntitiesWithinAabb(_box);
	entitiesInAABB.erase(std::remove_if(entitiesInAABB.begin(), entitiesInAABB.end(),
	                                    [_entityId](Entity* _entity) { return _entity->id == _entityId; }),
	                     entitiesInAABB.end());
	return entitiesInAABB;
}

std::vector<Entity*> EntityManager::GetEntitiesWithinAabbExcludingTypes(const AABB& _box,
                                                                        const std::vector<EntityType>& _excludedTypes) {
	std::vector<Entity*> exclusiveEntities;
	auto entitiesInAABB = GetEntitiesWithinAabb(_box);
	for (auto& entity : entitiesInAABB) {
		if (std::find(_excludedTypes.begin(), _excludedTypes.end(), entity->type) == _excludedTypes.end()) {
			exclusiveEntities.push_back(entity);
		}
	}
	return exclusiveEntities;
}

std::vector<Entity*> EntityManager::GetCollidablesWithinAabb(const AABB& _box, EntityId _excludedId) {
	// Get all entities within an AABB
	std::vector<Entity*> collidingEntities;

	// Normalize to block coordinates
	int blockMinX = MathHelper::FloorDouble((_box.minX - 2.0) / 16.0);
	int blockMinZ = MathHelper::FloorDouble((_box.minZ - 2.0) / 16.0);
	int blockMaxX = MathHelper::FloorDouble((_box.maxX + 2.0) / 16.0);
	int blockMaxZ = MathHelper::FloorDouble((_box.maxZ + 2.0) / 16.0);

	// Get our start and end bucket
	int bucketMinY = MathHelper::FloorDouble((_box.minY - 2.0) / 16.0);
	int bucketMaxY = MathHelper::FloorDouble((_box.maxY + 2.0) / 16.0);
	bucketMinY = std::max(0, bucketMinY);
	bucketMinY = std::min(9, bucketMinY);
	bucketMaxY = std::max(0, bucketMaxY);
	bucketMaxY = std::min(9, bucketMaxY);

	// Go through each block position
	for (int x = blockMinX; x <= blockMaxX; x++) {
		for (int z = blockMinZ; z <= blockMaxZ; z++) {
			auto it = entityContainers.find({ x, z });
			if (it == entityContainers.end())
				continue;
			auto& container = it->second;
			for (int by = bucketMinY; by <= bucketMaxY; by++) {
				// Get every entity within every bucket
				for (size_t i = 0; i < container.buckets[by].worldCollidableEntities.size(); i++) {
					auto& entityPtr = container.buckets[by].worldCollidableEntities[i];
					if (entityPtr->collider.Intersects(_box) && entityPtr->id != _excludedId)
						collidingEntities.push_back(entityPtr);
				}
			}
		}
	}
	return collidingEntities;
}

std::vector<Entity*> EntityManager::GetEntitiesWithinAabb(const AABB& _box) {
	// Get all entities within an AABB
	std::vector<Entity*> collidingEntities;

	// Normalize to block coordinates
	int blockMinX = MathHelper::FloorDouble((_box.minX - 2.0) / 16.0);
	int blockMinZ = MathHelper::FloorDouble((_box.minZ - 2.0) / 16.0);
	int blockMaxX = MathHelper::FloorDouble((_box.maxX + 2.0) / 16.0);
	int blockMaxZ = MathHelper::FloorDouble((_box.maxZ + 2.0) / 16.0);

	// Get our start and end bucket
	int bucketMinY = MathHelper::FloorDouble((_box.minY - 2.0) / 16.0);
	int bucketMaxY = MathHelper::FloorDouble((_box.maxY + 2.0) / 16.0);
	bucketMinY = std::max(0, bucketMinY);
	bucketMinY = std::min(9, bucketMinY);
	bucketMaxY = std::max(0, bucketMaxY);
	bucketMaxY = std::min(9, bucketMaxY);

	// Go through each block position
	for (int x = blockMinX; x <= blockMaxX; x++) {
		for (int z = blockMinZ; z <= blockMaxZ; z++) {
			auto it = entityContainers.find({ x, z });
			if (it == entityContainers.end())
				continue;
			auto& container = it->second;
			for (int by = bucketMinY; by <= bucketMaxY; by++) {
				// Get every entity within every bucket
				for (size_t i = 0; i < container.buckets[by].entities.size(); i++) {
					// Make sure the weak ptr is still valid
					auto& entityPtr = container.buckets[by].entities[i];
					if (entityPtr->collider.Intersects(_box))
						collidingEntities.push_back(entityPtr);
				}
			}
		}
	}
	return collidingEntities;
}

std::vector<Entity*> EntityManager::GetEntitiesWithinAabbOfType(const AABB& _box, EntityType& _type) {
	auto e = GetEntitiesWithinAabb(_box);
	std::vector<Entity*> typedEntities;

	for (auto& entity : e) {
		if (entity->type == _type)
			typedEntities.push_back(entity);
	}
	return typedEntities;
}

std::vector<Entity*> EntityManager::GetLivingEntitiesWithinAabb(const AABB& _box) {
	auto entitiesInAabb = GetEntitiesWithinAabb(_box);
	std::vector<Entity*> living;
	for (auto& entity : entitiesInAabb) {
		if (dynamic_cast<MobileEntity*>(entity))
			living.push_back(entity);
	}
	return living;
}

void EntityManager::CreateEntityFromNbt(Tag& _nbt) {
	// Load an entity from the nbt list
	std::string id = _nbt.compound["id"].GetString();
	if (id == "PigZombie") {
		ZombiePigEntity entity;
		entity.LoadFromNbt(_nbt);
		AddEntity(std::make_shared<ZombiePigEntity>(entity));
	}
	if (id == "Painting") {
		PaintingEntity entity;
		entity.LoadFromNbt(_nbt);
		AddEntity(std::make_shared<PaintingEntity>(entity));
	}
	if (id == "Minecart") {
		MinecartEntity entity;
		entity.LoadFromNbt(_nbt);
		AddEntity(std::make_shared<MinecartEntity>(entity));
	}
	if (id == "Boat") {
		BoatEntity entity;
		entity.LoadFromNbt(_nbt);
		AddEntity(std::make_shared<BoatEntity>(entity));
	}
	if (id == "Item") {
		ItemEntity entity({});
		entity.LoadFromNbt(_nbt);
		AddEntity(std::make_shared<ItemEntity>(entity));
	}
	if (id == "Zombie") {
		ZombieEntity entity;
		entity.LoadFromNbt(_nbt);
		AddEntity(std::make_shared<ZombieEntity>(entity));
	}
	if (id == "Cow") {
		CowEntity entity;
		entity.LoadFromNbt(_nbt);
		AddEntity(std::make_shared<CowEntity>(entity));
	}
	if (id == "Pig") {
		PigEntity entity;
		entity.LoadFromNbt(_nbt);
		AddEntity(std::make_shared<PigEntity>(entity));
	}
	if (id == "Sheep") {
		SheepEntity entity;
		entity.LoadFromNbt(_nbt);
		AddEntity(std::make_shared<SheepEntity>(entity));
	}
	if (id == "Chicken") {
		ChickenEntity entity;
		entity.LoadFromNbt(_nbt);
		AddEntity(std::make_shared<ChickenEntity>(entity));
	}
	if (id == "Skeleton") {
		SkeletonEntity entity;
		entity.LoadFromNbt(_nbt);
		AddEntity(std::make_shared<SkeletonEntity>(entity));
	}
	if (id == "Creeper") {
		CreeperEntity entity;
		entity.LoadFromNbt(_nbt);
		AddEntity(std::make_shared<CreeperEntity>(entity));
	}
	if (id == "Spider") {
		SpiderEntity entity;
		entity.LoadFromNbt(_nbt);
		AddEntity(std::make_shared<SpiderEntity>(entity));
	}
	if (id == "Arrow") {
		ArrowEntity entity;
		entity.LoadFromNbt(_nbt);
		AddEntity(std::make_shared<ArrowEntity>(entity));
	}
	if (id == "Snowball") {
		SnowballEntity entity;
		entity.LoadFromNbt(_nbt);
		AddEntity(std::make_shared<SnowballEntity>(entity));
	}
	if (id == "Slime") {
		SlimeEntity entity;
		entity.LoadFromNbt(_nbt);
		AddEntity(std::make_shared<SlimeEntity>(entity));
	}
	if (id == "FallingSand") {
		FallingBlockEntity entity(Vec3{}, BLOCK_AIR);
		entity.LoadFromNbt(_nbt);
		if (entity.block == BLOCK_SAND)
			entity.type = EntityType::FALLING_SAND;
		else if (entity.block == BLOCK_GRAVEL)
			entity.type = EntityType::FALLING_GRAVEL;
		AddEntity(std::make_shared<FallingBlockEntity>(entity));
	}
	if (id == "PrimedTnt") {
		TntEntity entity;
		entity.LoadFromNbt(_nbt);
		AddEntity(std::make_shared<TntEntity>(entity));
	}
	if (id == "Squid") {
		SquidEntity entity;
		entity.LoadFromNbt(_nbt);
		AddEntity(std::make_shared<SquidEntity>(entity));
	}
	if (id == "Wolf") {
		WolfEntity entity;
		entity.LoadFromNbt(_nbt);
		AddEntity(std::make_shared<WolfEntity>(entity));
	}
	if (id == "Ghast") {
		GhastEntity entity;
		entity.LoadFromNbt(_nbt);
		AddEntity(std::make_shared<GhastEntity>(entity));
	}
}

std::vector<Tag> EntityManager::CollectEntitiesForSave(Int2 _cpos, bool _clearCollectedEntities) {
	// Collect entities for this chunk coordinates (basically our entity container)
	// We then serialize these entities and return the vector of nbt tags
	// We mark the entities as dead for cleanup afterwards
	std::vector<Tag> collectedEntities;

	auto it = entityContainers.find(_cpos);
	if (it == entityContainers.end())
		return collectedEntities;

	auto& container = it->second;
	for (size_t i = 0; i < container.buckets.size(); i++) {
		auto& bucket = container.buckets[i];
		for (auto& entityPtr : bucket.entities) {
			// Is this entity dead but not collected?
			if (entityPtr->isDead)
				continue; // We are dead so no save
			if (entityPtr->type == EntityType::PLAYER)
				continue; // players cannot be saved
			if (_clearCollectedEntities)
				entityPtr->isDead = true; // Mark the entity as dead for cleanup
			auto compound = entityPtr->SerializeToNbt();
			if (!compound)
				continue; // If something went wrong abort save
			collectedEntities.push_back(*compound);
		}
	}

	if (_clearCollectedEntities)
		EraseContainer(_cpos);
	else
		PruneEmptyContainer(_cpos);

	return collectedEntities;
}

std::optional<std::string> EntityManager::GetEntityNbtId(EntityType _type) {
	switch (_type) {
	case EntityType::ITEM:
		return "Item";
	case EntityType::BOAT:
		return "Boat";
	case EntityType::LIT_TNT:
		return "PrimedTnt";
	case EntityType::ARROW:
		return "Arrow";
	case EntityType::THROWN_SNOWBALL:
		return "Snowball";
	case EntityType::PAINTING:
		return "Painting";
	case EntityType::CREEPER:
		return "Creeper";
	case EntityType::SKELETON:
		return "Skeleton";
	case EntityType::SPIDER:
		return "Spider";
	case EntityType::GIANT_ZOMBIE:
		return "Giant";
	case EntityType::ZOMBIE:
		return "Zombie";
	case EntityType::SLIME:
		return "Slime";
	case EntityType::GHAST:
		return "Ghast";
	case EntityType::ZOMBIE_PIGMAN:
		return "PigZombie";
	case EntityType::PIG:
		return "Pig";
	case EntityType::SHEEP:
		return "Sheep";
	case EntityType::COW:
		return "Cow";
	case EntityType::CHICKEN:
		return "Chicken";
	case EntityType::SQUID:
		return "Squid";
	case EntityType::WOLF:
		return "Wolf";

	// Vanilla only has ONE minecart entity/string
	// There is a type field in the nbt itself
	case EntityType::MINECART:
	case EntityType::STORAGE_MINECART:
	case EntityType::FURNACE_MINECART:
		return "Minecart";

	// Same deal as minecarts
	// not a separate string.
	case EntityType::FALLING_SAND:
	case EntityType::FALLING_GRAVEL:
		return "FallingSand";

	// Players have a special case!
	case EntityType::PLAYER:
		return "Player";

	// These have no mapping!
	case EntityType::NONE:
	case EntityType::FISH:
	case EntityType::FIREBALL:
	case EntityType::THROWN_EGG:
	case EntityType::FISHING_BOBBER:
	case EntityType::LIGHTNING_BOLT:
	default:
		return std::nullopt;
	}
}