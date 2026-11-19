/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */

#pragma once

#include "blocks/block_properties.h"
#include "entities.h"
#include "entity.h"
#include "java_random.h"
#include "numeric_structs.h"

struct LightningEntity : public Entity {
	int lightningState = 0;
	int timeAlive = 0;

	LightningEntity(WorldManager* _world, Vec3 _position) : Entity() {
		type = EntityType::LIGHTNING_BOLT;
		stepHeight = 0.0f;
		this->world = _world;
		this->Teleport(_position);

		lightningState = 2;
		timeAlive = rand.NextInt(3) + 1;

		TryCatchFire();
	}
	void TryCatchFire();
	void Tick() override;
};