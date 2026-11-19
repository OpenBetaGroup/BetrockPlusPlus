/*
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once
#include "damage_types.h"
#include "entities.h"
#include "entity.h"
#include "entity_zombie.h"
#include "nbt/nbt_tag.h"
#include <memory>
#include <optional>

struct ZombiePigEntity : public ZombieEntity {
	bool angry = false;

	ZombiePigEntity() : ZombieEntity() {
		type = EntityType::ZOMBIE_PIGMAN;
		isImmuneToFire = true;
	}
	~ZombiePigEntity() = default;

	void BecomeAngryAt(Entity* _entity);

	void OnDeath(Entity* _killer, DamageType _type) override;
	bool CanSpawnAt() override;
	void Tick() override;
	bool AttackEntityFrom(Entity* _entity, int _damage, DamageType _type) override;
	std::shared_ptr<Entity> FindPlayerToAttack() override;
	std::optional<Tag> SerializeToNbt() override;
	void LoadFromNbt(Tag& _nbt) override;
};