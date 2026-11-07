/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#include "entity_mob.h"

void MobEntity::OnDeath(Entity* /*_killer*/, DamageType /*_type*/) {
	// no op, drop items
	return;
}

bool MobEntity::TryDespawn() {
	auto player = entityManager->GetClosestPlayerWithin(position, -1);
	if (this->EntityAlive() && player) {
		double distance = position.DistanceSquared(player->position);

		// > 128 blocks away, despawn
		if (distance > 16384.0) {
			isDead = true;
			return true;
		}

		// > 32 blocks away? Try to despawn with a 1/800 chance
		if (this->age > 600 && rand.NextInt(800) == 0) {
			if (distance < 1024.0) {
				// We are 32 blocks or closer to a player
				age = 0;
				return false;
			}
			isDead = true;
			return true;
		}
	}
	return false;
}

void MobEntity::UpdateAIState() {
	if (EntityAlive()) {
		Wander();
		if (!FollowPath())
			UpdateState();
		randomYawVelocity *= 0.9f;
	}
}

void MobEntity::Tick() {
	MobileEntity::Tick();
	TryDespawn();
}

void MobEntity::FaceEntity(MobileEntity& _entity, float _maxYaw, float _maxPitch) {
	auto dx = _entity.position.x - position.x;
	auto dz = _entity.position.z - position.z;

	auto desiredYaw = (std::atan2(dz, dx) * (180 / JavaMath::PI)) - 90.0;

	auto horizontalDistance = std::sqrt((dx * dx) + (dz * dz));
	auto targetEyeHeight = _entity.position.y + _entity.GetEyeHeight();
	auto thisEyeHeight = position.y + GetEyeHeight();

	auto dy = thisEyeHeight - targetEyeHeight;
	auto desiredPitch = (std::atan2(dy, horizontalDistance) * (180 / JavaMath::PI)) * -1.0;

	rotationPitch = -GetDesiredRotation(rotationPitch, desiredPitch, _maxPitch);
	rotationYaw = GetDesiredRotation(rotationYaw, desiredYaw, _maxYaw);
}

void MobEntity::UpdateState() {
	// Reset our motion
	input.x = 0.0f;
	input.y = 0.0f;

	// Try and look at a player
	// 2 percent chance
	if (rand.NextFloat() < 0.02f) {
		auto nearestPlayer = this->entityManager->GetClosestPlayerWithin(this->position, 8);
		if (nearestPlayer) {
			ticksToFollowTarget = 10 + rand.NextInt(20);
			target = nearestPlayer;
		} else {
			randomYawVelocity = (rand.NextFloat() - 0.5) * 20.0f;
		}
	}

	auto thisTarget = target.lock();
	if (thisTarget) {
		// Turns towards our target
		FaceEntity(static_cast<MobileEntity&>(*thisTarget), 10.0, 40.0);

		// If our ticks to look at this target expired, stop looking
		// If our target is dead or too far, stop looking
		ticksToFollowTarget = std::max(ticksToFollowTarget - 1.0, 0.0);
		if (ticksToFollowTarget == 0.0 || thisTarget->isDead || (position.DistanceSquared(thisTarget->position) > 64.0))
			target.reset();
		return;
	}

	if (rand.NextFloat() < 0.05f)
		randomYawVelocity = (rand.NextFloat() - 0.5) * 20.0f;
	rotationYaw += randomYawVelocity;
	rotationPitch = defaultPitch;
}

void MobEntity::Wander() {
	bool hasPath = !currentPath.empty();

	bool shouldWander = (!hasPath && rand.NextInt(80) == 0) || rand.NextInt(80) == 0;
	if (!shouldWander)
		return;

	Int3 origin = { MathHelper::FloorDouble(position.x), MathHelper::FloorDouble(position.y),
		            MathHelper::FloorDouble(position.z) };

	bool found = false;
	Int3 best{};
	float bestWeight = -99999.0f;

	// Sample 10 random points
	for (int i = 0; i < 10; i++) {
		Int3 candidate = { origin.x + rand.NextInt(13) - 6, origin.y + rand.NextInt(7) - 3,
			               origin.z + rand.NextInt(13) - 6 };

		float weight = GetWanderWeight(candidate);
		if (weight > bestWeight) {
			bestWeight = weight;
			best = candidate;
			found = true;
		}
	}

	if (found)
		SetGoal(best);
}

float MobEntity::GetWanderWeight(Int3 _pos) {
	// Grass underfoot always wins
	Int3 below = _pos;
	below.y -= 1;
	if (world->GetBlockId(below) == BlockType::BLOCK_GRASS)
		return 10.0f;

	// Otherwise score by how lit the spot is
	int light = std::max(world->GetSkyLight(_pos), world->GetBlockLight(_pos));
	return (float(light) / 15.0f) - 0.5f;
}
