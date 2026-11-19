/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#include "entity_skeleton.h"
#include "entity_arrow.h"
#include "entity_manager.h"
#include "entity_mobile.h"
#include "enums/items.h"
#include "helpers/java/java_math.h"
#include "java_random.h"
#include "numeric_structs.h"
#include <cmath>
#include <memory>

void SkeletonEntity::OnDeath(Entity* /*_killer*/, DamageType /*_type*/) {
	// Drop arrows
	auto targetItem = Items::Id::ARROW;
	int itemCount = this->rand.NextInt(3);

	for (int i = 0; i < itemCount; i++) {
		DropItemAtEntity(targetItem, 1);
	}

	// Drop bones
	targetItem = Items::Id::BONE;
	itemCount = this->rand.NextInt(3);

	for (int i = 0; i < itemCount; i++) {
		DropItemAtEntity(targetItem, 1);
	}
}

void SkeletonEntity::TryAttackEntity(Entity& _target, float _distance) {
	if (_distance >= 10.0f)
		return;

	auto dist = _target.position - this->position;
	if (this->attackTime == 0) {
		auto self = entityManager->GetEntityByIdShared(this->id);
		auto mobileSelf = std::dynamic_pointer_cast<MobileEntity>(self);
		auto mobileTarget = dynamic_cast<MobileEntity*>(&_target);

		auto arrow = std::make_shared<ArrowEntity>(mobileSelf);
		arrow->position.y++;
		arrow->RebuildCollider();
		double velY = mobileTarget->position.y + mobileTarget->GetEyeHeight() - 0.2 - arrow->position.y;
		float velYCompensation = std::sqrt(dist.x * dist.x + dist.z * dist.z) * 0.2f;
		arrow->SetArrowHeading({ dist.x, velY + velYCompensation, dist.z }, 0.6f, 12.0f);
		entityManager->AddEntity(arrow);

		this->attackTime = 30;
	}

	this->rotationYaw = std::atan2(dist.x, dist.z) * 180.0 / JavaMath::PI - 90;
	this->hasAttacked = true;
}