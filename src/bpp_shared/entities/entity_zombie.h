/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#pragma once
#include "damage_types.h"
#include "entities.h"
#include "entity.h"
#include "entity_hostile.h"

struct ZombieEntity : public HostileEntity {
	ZombieEntity() : HostileEntity() {
		type = EntityType::ZOMBIE;
		width = 0.6f;
		height = 1.8f;
		burnInDaylight = true;

		// Speed / damage
		movementSpeed = 0.5f;
		attackStrength = 5;
	}
	~ZombieEntity() = default;
	void OnDeath(Entity* _killer, DamageType _type) override;
};