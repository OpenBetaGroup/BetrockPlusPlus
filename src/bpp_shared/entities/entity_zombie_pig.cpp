/*
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>-
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "entity_zombie_pig.h"
#include "entities.h"

void ZombiePigEntity::OnDeath(Entity* /*_killer*/, DamageType /*_type*/) {
	// Drop 0-2 cooked porkchops
	auto targetItem = Items::Id::PORKCHOP_COOKED;
	int itemCount = this->rand.NextInt(3);

	for (int i = 0; i < itemCount; i++) {
		DropItemAtEntity(targetItem, 1);
	}
}

bool ZombiePigEntity::CanSpawnAt() {
	return world && world->IsAABBClear(collider) && world->GetCollidingBoundingBoxes(collider, this).empty() &&
	       !world->IsLiquidInAabb(collider);
}

void ZombiePigEntity::Tick() {
	movementSpeed = attackTarget.lock() ? 0.95f : 0.5f;

	ZombieEntity::Tick();
}

void ZombiePigEntity::BecomeAngryAt(Entity* _entity) {
	attackTarget = entityManager->GetEntityByIdShared(_entity->id);
	angry = true;
}

bool ZombiePigEntity::AttackEntityFrom(Entity* _entity, int _damage, DamageType _type) {
	if (_entity && _entity->type == EntityType::PLAYER) {
		auto friends = world->entityManager.GetEntitiesWithinAabbOfType(collider.Expand(32.0, 32.0, 32.0), this->type);

		for (const auto& entity : friends) {
			static_cast<ZombiePigEntity*>(entity)->BecomeAngryAt(_entity);
		}

		BecomeAngryAt(_entity);
	}

	return ZombieEntity::AttackEntityFrom(_entity, _damage, _type);
}

std::shared_ptr<Entity> ZombiePigEntity::FindPlayerToAttack() {
	if (angry) {
		return ZombieEntity::FindPlayerToAttack();
	}

	return nullptr;
}

std::optional<Tag> ZombiePigEntity::SerializeToNbt() {
	auto tag = ZombieEntity::SerializeToNbt();
	if (!tag)
		return std::nullopt;

	Tag angerTag;
	angerTag.type = TAG_SHORT;
	angerTag.name = "Anger";

	// Vanilla uses a random integer as "anger level" but doesn't do anything with that number other than checking if it's not 0
	// We don't have to replicate that, so we just use a bool and convert it to a short when saving to keep NBT compatibility
	// 400 was chosen as it's the base for the random number in vanilla
	angerTag.shortValue = angry ? 400 : 0;

	tag->compound["Anger"] = angerTag;

	return tag;
}

void ZombiePigEntity::LoadFromNbt(Tag& _nbt) {
	ZombieEntity::LoadFromNbt(_nbt);

	if (_nbt.Has("Anger")) {
		angry = _nbt.Get("Anger").GetShort() > 0;
	}
}