/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#pragma once
#include "entity_hostile.h"

struct SpiderEntity : public HostileEntity {
	SpiderEntity() : HostileEntity() {
		type = EntityType::SPIDER;
		width = 1.4f;
		height = 0.9f;
		movementSpeed = 0.8f;
		burnInDaylight = false;
		canTriggerWalking = false;
	}
	~SpiderEntity() = default;
	bool IsOnLadder() override;
	void OnDeath(Entity* _killer, DamageType _type) override;
	void TryAttackEntity(Entity& _target, float _distance) override;
	std::shared_ptr<Entity> FindPlayerToAttack() override;
};