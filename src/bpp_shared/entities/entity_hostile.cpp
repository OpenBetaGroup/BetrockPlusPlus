/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#include "entity_hostile.h"

std::shared_ptr<Entity> HostileEntity::FindPlayerToAttack() {
	auto candidate = entityManager->GetClosestPlayerWithin(position, 16);
	if (!candidate)
		return nullptr;

	Vec3 eyeFrom = { position.x, position.y + GetEyeHeight(), position.z };
	Vec3 eyeTo = { candidate->position.x, candidate->position.y + candidate->height * 0.85f, candidate->position.z };
	if (!HasLineOfSight(eyeFrom, eyeTo))
		return nullptr;

	return candidate;
}

bool HostileEntity::AttackEntityFrom(Entity* _entity, int _damage, DamageType _type) {
	if (MobileEntity::AttackEntityFrom(_entity, _damage, _type)) {
		if (_entity != this->passenger.lock().get() && _entity != this->vehicle.lock().get()) {
			if (_entity && _entity != this) {
				this->attackTarget = entityManager->GetEntityByIdShared(_entity->id);
			}
		}
		return true;
	}
	return false;
}

void HostileEntity::TryAttackEntity(Entity& _target, float _distance) {
	// Check if our attack time and distance are valid, make sure our Y axis overlap
	if (attackTime <= 0 && _distance < 2.0f && _target.collider.maxY > collider.minY &&
	    _target.collider.minY < collider.maxY) {
		attackTime = 20;
		_target.AttackEntityFrom(this, attackStrength, DamageType::Punch);
	}
}

void HostileEntity::Tick() {
	if (world && world->IsDay() && this->burnInDaylight) {
		Int3 blockPos = { MathHelper::FloorDouble(position.x), MathHelper::FloorDouble(position.y),
			              MathHelper::FloorDouble(position.z) };

		float brightness = GetEntityBrightnessValue();

		Chunk* chunk = world->GetChunkRaw({ blockPos.x >> 4, blockPos.z >> 4 });
		bool seesSky = chunk && chunk->CanBlockSeeSky({ blockPos.x & 15, blockPos.y, blockPos.z & 15 });

		if (brightness > 0.5f && seesSky && rand.NextFloat() * 30.0f < (brightness - 0.4f) * 2.0f) {
			fireTicks = 300;
		}
	}

	MobileEntity::Tick();

	TryDespawn();
}

void HostileEntity::UpdateAIState() {
	if (!EntityAlive())
		return;

	// Only a couple mobs use this!
	hasAttacked = false;

	auto currentTarget = attackTarget.lock();

	// Get our target
	if (!currentTarget) {
		auto found = FindPlayerToAttack();
		if (found) {
			attackTarget = found;
			currentTarget = found;
			Int3 goal = { MathHelper::FloorDouble(found->position.x), MathHelper::FloorDouble(found->position.y),
				          MathHelper::FloorDouble(found->position.z) };
			SetGoal(goal);
		}
	} else if (currentTarget->isDead) {
		attackTarget.reset();
		currentTarget = nullptr;
	} else {
		float distance = float(position.Distance(currentTarget->position));
		Vec3 eyeFrom = { position.x, position.y + GetEyeHeight(), position.z };
		Vec3 eyeTo = { currentTarget->position.x, currentTarget->position.y + currentTarget->height * 0.85f,
			           currentTarget->position.z };
		if (HasLineOfSight(eyeFrom, eyeTo))
			TryAttackEntity(*currentTarget, distance);
		else
			OnTargetLostSight(*currentTarget, distance);
	}

	// Decide whether to repath
	bool hasPath = !currentPath.empty();
	if (hasAttacked || !currentTarget || (hasPath && rand.NextInt(20) != 0)) {
		if (!hasAttacked)
			Wander();
	} else {
		Int3 goal = { MathHelper::FloorDouble(currentTarget->position.x),
			          MathHelper::FloorDouble(currentTarget->position.y),
			          MathHelper::FloorDouble(currentTarget->position.z) };
		SetGoal(goal);
	}

	rotationPitch = 0.0;

	// Follow our path and strafe if we are close and have attacked
	if (FollowPath()) {
		if (hasAttacked && currentTarget) {
			double dx = currentTarget->position.x - position.x;
			double dz = currentTarget->position.z - position.z;
			float previousYaw = rotationYaw;
			rotationYaw = float(std::atan2(dz, dx) * 180.0 / JavaMath::PI) - 90.0f;
			float angleDiff = (previousYaw - rotationYaw + 90.0f) * (JavaMath::PI / 180.0f);
			float forward = input.y;
			input.x = -std::sin(angleDiff) * forward;
			input.y = std::cos(angleDiff) * forward;
		}

		if (currentTarget)
			FaceEntity(static_cast<MobileEntity&>(*currentTarget), 30.0f, 30.0f);
	} else {
		UpdateState(); // Idle if we dont have a path
	}

	randomYawVelocity *= 0.9f;
}

float HostileEntity::GetWanderWeight(Int3 _pos) {
	if (!world)
		return -99999.0f;

	int combinedLight = world->GetBlockLightFull(_pos);
	float brightness = float(combinedLight) / 15.0f;

	// TODO: needs brightness curve!
	return 0.5f - brightness;
}

bool HostileEntity::CanSpawnAt() {
	if (!world)
		return false;

	auto fd = MathHelper::FloorDouble;
	Int3 footPos = { fd(position.x), fd(collider.minY), fd(position.z) };

	// This checks the raw skylight
	int skyLight = world->GetSkyLight(footPos);
	if (skyLight > rand.NextInt(32))
		return false;

	// This checks the ACTUAL light level given the time of day
	// Thunderstorms darken skylight
	int combinedLight;
	if (world->weatherSystem.IsThundering()) {
		int savedOffset = world->skylightOffset;
		world->skylightOffset = 10;
		combinedLight = world->GetBlockLightFull(footPos);
		world->skylightOffset = savedOffset;
	} else {
		combinedLight = world->GetBlockLightFull(footPos);
	}
	if (combinedLight > rand.NextInt(8))
		return false;

	return MobileEntity::CanSpawnAt();
}