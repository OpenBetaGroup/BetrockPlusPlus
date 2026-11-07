/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */

#include "entity_fireball.h"
#include "raycast.h"

void FireballEntity::Tick() {
	this->prevRotationYaw = this->rotationYaw;
	this->prevRotationPitch = this->rotationPitch;

	Entity::Tick();

	this->fireTicks = 10;
	this->ticksInAir++;

	Vec3 start = this->position;
	Vec3 end = { this->position.x + this->velocity.x, this->position.y + this->velocity.y,
		         this->position.z + this->velocity.z };
	RayCastResult blockHit = Raycast::Raycast(*world, start, end, RayCastMode::IGNORE_FLUIDS,
	                                          /*_ignoreNonCollidable=*/false);
	if (blockHit.hit)
		end = blockHit.hitPosition;

	// Then against entities, only up to the block we hit
	auto ownerPtr = this->owner.lock();
	Entity* hitEntity = nullptr;
	double closestDistance = 0.0;
	AABB searchBox = this->collider.AddCoord(this->velocity.x, this->velocity.y, this->velocity.z).Expand(1.0, 1.0, 1.0);
	for (const auto& candidate : entityManager->GetEntitiesWithinAabbExcluding(searchBox, this->id)) {
		if (!candidate->CanBeCollidedWith())
			continue;

		// Don't shoot ourselves on the way out
		if (ownerPtr && candidate == ownerPtr.get() && this->ticksInAir < 25)
			continue;

		const double grow = 0.3;
		auto intercept = candidate->collider.Expand(grow, grow, grow).CalculateIntercept(start, end);
		if (!intercept)
			continue;

		double distance = start.Distance(intercept->point);
		if (distance < closestDistance || closestDistance == 0.0) {
			hitEntity = candidate;
			closestDistance = distance;
		}
	}

	if (blockHit.hit || hitEntity) {
		if (hitEntity) {
			// We dont do any damage but this will trigger the entity to be "irritated" at you
			hitEntity->AttackEntityFrom(ownerPtr.get(), 0, DamageType::None);
		}

		this->world->DoExplosion(nullptr, this->position, /*size=*/1.0, /*doFire=*/true);

		this->isDead = true;
	}

	// Move
	this->position.x += this->velocity.x;
	this->position.y += this->velocity.y;
	this->position.z += this->velocity.z;

	// Ease our rotation towards our direction of travel
	this->SetRotationBasedOnVelocity(this->velocity);

	while (this->rotationPitch - this->prevRotationPitch < -180.0f)
		this->prevRotationPitch -= 360.0f;
	while (this->rotationPitch - this->prevRotationPitch >= 180.0f)
		this->prevRotationPitch += 360.0f;
	while (this->rotationYaw - this->prevRotationYaw < -180.0f)
		this->prevRotationYaw -= 360.0f;
	while (this->rotationYaw - this->prevRotationYaw >= 180.0f)
		this->prevRotationYaw += 360.0f;

	// Easing
	this->rotationPitch = this->prevRotationPitch + (this->rotationPitch - this->prevRotationPitch) * 0.2f;
	this->rotationYaw = this->prevRotationYaw + (this->rotationYaw - this->prevRotationYaw) * 0.2f;

	// Drag and gravity
	float drag = this->inWater ? 0.8f : 0.95f;
	this->velocity += this->acceleration;
	this->velocity *= double(drag);
	this->RebuildCollider();
}