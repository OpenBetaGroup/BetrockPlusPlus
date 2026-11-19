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
#include "entity_animal.h"
#include "enums/items.h"
#include "java_random.h"
#include <cstdint>

struct ChickenEntity : public AnimalEntity {
	uint32_t ticksUntilEgg = 0;
	ChickenEntity() : AnimalEntity() {
		type = EntityType::CHICKEN;
		width = 0.3f;
		height = 0.4f;
		SetMaxHealth(/*Health=*/4);
		RollEggTimer();
	}
	~ChickenEntity() = default;
	void OnDeath(Entity* _killer, DamageType _type) override;
	void UpdateFallState(float _movedY) override;
	void RollEggTimer() {
		ticksUntilEgg = uint32_t(this->rand.NextInt(6000) + 6000);
	}
	void Tick() override {
		AnimalEntity::Tick();
		// Chickens can fly
		if (!this->onGround && this->velocity.y < 0.0) {
			this->velocity.y *= 0.6;
		}

		// Lay EGG
		if (ticksUntilEgg-- <= 0) {
			this->DropItemAtEntity(Items::EGG, 1);
			this->RollEggTimer();
		}
	}
};