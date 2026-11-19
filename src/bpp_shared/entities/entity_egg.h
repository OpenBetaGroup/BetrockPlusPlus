/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#pragma once
#include "entities.h"
#include "entity_chicken.h"
#include "entity_manager.h"
#include "entity_mobile.h"
#include "entity_throwable.h"
#include "java_random.h"
#include "numeric_structs.h"
#include <cstdint>
#include <memory>

struct EggEntity : public ThrowableEntity {
	// Default
	EggEntity() : ThrowableEntity() {
		type = EntityType::THROWN_EGG;
	}

	// Spawn from position
	EggEntity(Vec3 _position) : ThrowableEntity(_position) {
		type = EntityType::THROWN_EGG;
	}

	// Spawn from entity
	EggEntity(std::shared_ptr<MobileEntity> _owner) : ThrowableEntity(_owner) {
		type = EntityType::THROWN_EGG;
	}

	void OnHit(Vec3 /*_pos*/) override {
		if (rand.NextInt(8) != 0)
			return;

		uint8_t count = rand.NextInt(32) == 0 ? 4 : 1;

		for (int i = 0; i < count; i++) {
			std::shared_ptr<ChickenEntity> chicken = std::make_shared<ChickenEntity>();
			chicken->Teleport(this->position, { this->rotationYaw, 0 });
			entityManager->AddEntity(chicken);
		}
	};
};