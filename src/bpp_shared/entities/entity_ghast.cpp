/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#include "entity_ghast.h"
#include "entity_fireball.h"
#include "entity_manager.h"
#include "gamerules.h"
#include "world.h"
#include <algorithm>
#include <cmath>

void GhastEntity::Tick() {
	// NOTE:
	// Same quirk as slimes and squids :p
	if (Gamerule::Get<bool>(Gamerule::Id::AccurateDespawnRules))
		this->age = 0;

	HostileEntity::Tick();
}

void GhastEntity::UpdateAIState() {
	if (!EntityAlive())
		return;

	// TODO: Remove if difficult > 0

	double dx = waypoint.x - position.x;
	double dy = waypoint.y - position.y;
	double dz = waypoint.z - position.z;
	double dist = std::sqrt(dx * dx + dy * dy + dz * dz);

	if (dist < 1.0 || dist > 60.0) {
		waypoint.x = position.x + double((rand.NextFloat() * 2.0f - 1.0f) * 16.0f);
		waypoint.y = position.y + double((rand.NextFloat() * 2.0f - 1.0f) * 16.0f);
		waypoint.z = position.z + double((rand.NextFloat() * 2.0f - 1.0f) * 16.0f);
	}

	if (courseChangeCooldown-- <= 0) {
		courseChangeCooldown += rand.NextInt(5) + 2;
		if (dist > 0.0 && IsCourseTraversable(dist)) {
			velocity.x += dx / dist * 0.1;
			velocity.y += dy / dist * 0.1;
			velocity.z += dz / dist * 0.1;
		} else {
			waypoint = position;
		}
	}

	// TODO: Again, doesn't the base entity already supply "target"?
	auto target = targetedEntity.lock();
	if (target && target->isDead) {
		targetedEntity.reset();
		target = nullptr;
	}
	if (!target || aggroCooldown-- <= 0) {
		target = entityManager->GetClosestPlayerWithin(position, 100);
		targetedEntity = target;
		if (target)
			aggroCooldown = 20;
	}

	constexpr double AGGRO_RANGE = 64.0;
	if (target && position.DistanceSquared(target->position) < AGGRO_RANGE * AGGRO_RANGE) {
		// Aim at the middle of the target from the middle of us
		double toX = target->position.x - position.x;
		double toY = (target->collider.minY + double(target->height) / 2.0) - (position.y + double(height) / 2.0);
		double toZ = target->position.z - position.z;
		rotationYaw = float(-std::atan2(toX, toZ) * 180.0 / JavaMath::PI);

		Vec3 eyeFrom = { position.x, position.y + GetEyeHeight(), position.z };
		Vec3 eyeTo = { target->position.x, target->position.y + target->height * 0.85f, target->position.z };
		if (HasLineOfSight(eyeFrom, eyeTo)) {
			attackCounter++;
			if (attackCounter == 20) {
				ShootFireball(toX, toY, toZ);
				attackCounter = -40;
			}
		} else if (attackCounter > 0) {
			attackCounter--;
		}
	} else {
		// Nothing to shoot at, face where we are drifting
		rotationYaw = float(-std::atan2(velocity.x, velocity.z) * 180.0 / JavaMath::PI);
		if (attackCounter > 0)
			attackCounter--;
	}

	UpdateMetadata<int8_t>(attackingFlag, int8_t(attackCounter > 10 ? 1 : 0));
}

bool GhastEntity::IsCourseTraversable(double _distance) {
	double stepX = (waypoint.x - position.x) / _distance;
	double stepY = (waypoint.y - position.y) / _distance;
	double stepZ = (waypoint.z - position.z) / _distance;

	AABB box = collider;
	for (int i = 1; double(i) < _distance; i++) {
		box = box.Offset(stepX, stepY, stepZ);
		if (!world->GetCollidingBoundingBoxes(box, this).empty())
			return false;
	}
	return true;
}

void GhastEntity::ShootFireball(double _dx, double _dy, double _dz) {
	auto self = std::dynamic_pointer_cast<MobileEntity>(entityManager->GetEntityByIdShared(this->id));
	if (!self)
		return;

	auto fireball = std::make_shared<FireballEntity>(self, Vec3{ _dx, _dy, _dz });

	constexpr double SPAWN_DISTANCE = 4.0;
	Vec3 look = GetLookVector(rotationYaw, rotationPitch);
	fireball->position = { position.x + look.x * SPAWN_DISTANCE, position.y + double(height) / 2.0 + 0.5,
		                   position.z + look.z * SPAWN_DISTANCE };
	fireball->RebuildCollider();

	entityManager->AddEntity(fireball);
}

void GhastEntity::TickPhysics() {
	// TODO: Don't we already get this via the base "inLava"?
	bool currentlyInLava = world->IsMaterialInAabb(GetLavaCollider(), Material::Lava());
	if (inWater) {
		Move(this->velocity);
		velocity *= 0.8;
	} else if (currentlyInLava) {
		Move(this->velocity);
		velocity *= 0.5;
	} else {
		float friction = 0.91f;
		if (onGround) {
			friction = 0.546f;
			auto below = world->GetBlockId({ MathHelper::FloorDouble(position.x), MathHelper::FloorDouble(position.y) - 1,
			                                 MathHelper::FloorDouble(position.z) });
			if (below > BLOCK_AIR)
				friction = Blocks::blockProperties[below < BLOCK_MAX ? below : BLOCK_MAX].slipperiness * 0.91f;
		}

		Move(this->velocity);
		velocity *= friction;
	}
}

void GhastEntity::OnDeath(Entity* /*_killer*/, DamageType /*_type*/) {
	int itemCount = this->rand.NextInt(3);
	for (int i = 0; i < itemCount; i++) {
		DropItemAtEntity(Items::Id::GUNPOWDER, 1);
	}
}

bool GhastEntity::CanSpawnAt() {
	// TODO: requires difficulty > 0
	return rand.NextInt(20) == 0 && MobEntity::CanSpawnAt();
}

void GhastEntity::EncodeMetadata(std::vector<PacketData::EntityMetadata::DataEntry>& _metadata) {
	Entity::EncodeMetadata(_metadata);

	_metadata.push_back({ .type = PacketData::EntityMetadata::BYTE, .index = 16, .value = attackingFlag });
}

bool GhastEntity::DecodeMetadata(const std::vector<PacketData::EntityMetadata::DataEntry>& _metadata) {
	if (!Entity::DecodeMetadata(_metadata)) {
		return false;
	}

	if (auto* raw = FindMetadata<int8_t>(_metadata, PacketData::EntityMetadata::BYTE, 16)) {
		attackingFlag = *raw;
		return true;
	}
	return false;
}
