/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#include "entity_squid.h"
#include "entity_animal.h"
#include "enums/items.h"
#include "gamerules.h"
#include "helpers/java/java_math.h"
#include <cmath>

bool SquidEntity::CanSpawnAt() {
	if (!this->world)
		return false;

	return this->world->IsAABBClear(this->collider);
}

void SquidEntity::UpdateAIState() {
	if (this->rand.NextInt(50) == 0 || !this->inWater || randomVelocity == Vec3{}) {
		float radians = this->rand.NextFloat() * JavaMath::PI_FLOAT * 2.0f;
		randomVelocity.x = MathHelper::Cos(radians) * 0.2f;
		randomVelocity.y = -0.1f + this->rand.NextFloat() * 0.2f;
		randomVelocity.z = MathHelper::Sin(radians) * 0.2f;
	}
}

void SquidEntity::OnDeath(Entity* /*_killer*/, DamageType /*_type*/) {
	int itemCount = this->rand.NextInt(3) + 1;

	for (int i = 0; i < itemCount; i++) {
		DropItemAtEntity(Items::Id::DYE, 1);
	}
}

void SquidEntity::Tick() {
	// Despawn quirk :p
	if (Gamerule::Get<bool>(Gamerule::Id::AccurateDespawnRules))
		this->age = 0;
	AnimalEntity::Tick();

	// Rotation here is basically the "phase" of the squids cycle
	// Its in rads!
	rotation += rotationVelocity;
	if (rotation > JavaMath::PI_FLOAT * 2.0f) {
		rotation -= JavaMath::PI_FLOAT * 2.0f;
		if (this->rand.NextInt(10) == 0)
			rotationVelocity = 1.0f / (rand.NextFloat() + 1.0f) * 0.2f;
	}

	if (IsSwimming()) {
		if (rotation < JavaMath::PI_FLOAT) {
			float normalized = rotation / JavaMath::PI_FLOAT;
			if (normalized > 0.75f) {
				randomMotionSpeed = 1.0f;
			}
		} else {
			randomMotionSpeed *= 0.9f;
		}

		this->velocity = this->randomVelocity * this->randomMotionSpeed;

		this->rotationYaw += (-std::atan2(velocity.x, velocity.z) * 180.0f / JavaMath::PI_FLOAT - rotationYaw) * 0.1f;
		return;
	}

	// We are out of water
	velocity.x = 0.0;
	velocity.z = 0.0;
	velocity.y -= GRAVITY;
	velocity.y *= VERTICAL_FRICTION;
}