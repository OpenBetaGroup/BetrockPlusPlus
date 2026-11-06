/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#include "entity_spider.h"

void SpiderEntity::OnDeath(Entity* /*_killer*/, DamageType /*_type*/) {
	// Drop arrows
	auto targetItem = Items::Id::STRING;
	int itemCount = this->rand.NextInt(3);

	for (int i = 0; i < itemCount; i++) {
		DropItemAtEntity(targetItem, 1);
	}
}

bool SpiderEntity::IsOnLadder() {
	// Spiders climb on walls
	return this->collidedHorizontally;
}

void SpiderEntity::TryAttackEntity(Entity& _target, float _distance) {
	auto brightness = this->GetEntityBrightnessValue();

	if (brightness > 0.5f && rand.NextInt(100) == 0) {
		this->attackTarget.reset();
		return;
	}
	auto activeTarget = attackTarget.lock();
	if (!activeTarget)
		return;
	if (_distance > 2.0f && _distance < 6.0f && rand.NextInt(10) == 0) {
		if (this->onGround) {
			double dx = _target.position.x - position.x;
			double dz = _target.position.z - position.z;
			float dist = std::sqrt(dx * dx + dz * dz);
			this->velocity.x = dx / dist * 0.5f * 0.8 + this->velocity.x * 0.2;
			this->velocity.z = dz / dist * 0.5f * 0.8 + this->velocity.z * 0.2;
			this->velocity.y = 0.4;
			return;
		}
	}
	HostileEntity::TryAttackEntity(*activeTarget, _distance);
}

std::shared_ptr<Entity> SpiderEntity::FindPlayerToAttack() {
	auto brightness = this->GetEntityBrightnessValue();
	if (brightness >= 0.5f)
		return nullptr;

	auto candidate = entityManager->GetClosestPlayerWithin(position, 16);
	if (!candidate)
		return nullptr;

	Vec3 eyeFrom = { position.x, position.y + GetEyeHeight(), position.z };
	Vec3 eyeTo = { candidate->position.x, candidate->position.y + candidate->height * 0.85f, candidate->position.z };
	if (!HasLineOfSight(eyeFrom, eyeTo))
		return nullptr;

	return candidate;
}