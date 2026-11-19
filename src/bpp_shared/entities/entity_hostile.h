/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#pragma once
#include "damage_types.h"
#include "entity.h"
#include "entity_mob.h"
#include "numeric_structs.h"
#include <memory>

struct HostileEntity : public MobEntity {
	std::weak_ptr<Entity> attackTarget;
	bool hasAttacked = false;
	bool burnInDaylight = false;
	int attackStrength = 2;

	HostileEntity() : MobEntity() {
		SetMaxHealth(20);
		this->isHostile = true;
	}
	~HostileEntity() = default;

	bool CanSpawnAt() override;
	float GetWanderWeight(Int3 _pos) override;
	virtual void Tick() override;
	void UpdateAIState() override;
	virtual std::shared_ptr<Entity> FindPlayerToAttack();
	virtual void TryAttackEntity(Entity& _target, float _distance);
	virtual void OnTargetLostSight(Entity& /*_target*/, float /*_distance*/) {}
	bool AttackEntityFrom(Entity* _entity, int _damage, DamageType _type) override;
};