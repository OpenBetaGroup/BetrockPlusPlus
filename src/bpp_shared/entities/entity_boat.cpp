/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#include "entity_boat.h"
#include "entity_manager.h"
#include "helpers/java/java_math.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>

void BoatEntity::DropAsItems() {
	if (auto rider = passenger.lock())
		rider->UnmountEntity();
	if (Gamerule::Get<bool>("boat_drop_fix")) {
		DropItemAtEntity(Items::BOAT, 1);
	} else {
		for (int i = 0; i < 3; i++)
			DropItemAtEntity(ItemId(BlockType::BLOCK_PLANKS), 1);
		for (int i = 0; i < 2; i++)
			DropItemAtEntity(Items::Id::STICK, 1);
	}
	isDead = true;
}

bool BoatEntity::AttackEntityFrom(Entity* _entity, int _damage, DamageType _type) {
	if (isDead)
		return true;

	Entity::AttackEntityFrom(_entity, _damage, _type);

	forwardDirection = -forwardDirection;
	shakeTimer = 10;
	damageTaken += _damage * 10;

	if (damageTaken > 40) {
		DropAsItems();
	}

	return true;
}

void BoatEntity::OnPlayerInteract(PlayerEntity* _entity) {
	if (!_entity)
		return;

	auto rider = passenger.lock();
	if (rider && rider.get() != _entity) {
		// Someone else is already riding
		return;
	}

	if (_entity == rider.get()) {
		_entity->UnmountEntity();
		return;
	}

	auto selfPtr = entityManager->GetEntityByIdShared(this->id);
	if (selfPtr)
		_entity->MountEntity(selfPtr);
}

void BoatEntity::Tick() {
	Entity::Tick();
	if (shakeTimer > 0)
		shakeTimer--;
	if (damageTaken > 0)
		damageTaken--;

	Vec3 prevPos = position;

	// Sample 5 horizontal slices of our own bounding box to estimate how submerged we are
	const int sliceCount = 5;
	double submersion = 0.0;
	for (int i = 0; i < sliceCount; i++) {
		double sliceMinY = collider.minY + (collider.maxY - collider.minY) * double(i + 0) / double(sliceCount) - 0.125;
		double sliceMaxY = collider.minY + (collider.maxY - collider.minY) * double(i + 1) / double(sliceCount) - 0.125;
		AABB slice = { collider.minX, sliceMinY, collider.minZ, collider.maxX, sliceMaxY, collider.maxZ };
		if (world->IsAabbInFluidLevel(slice, Material::Water()))
			submersion += 1.0 / double(sliceCount);
	}

	// Buoyancy
	if (submersion < 1.0) {
		double t = submersion * 2.0 - 1.0;
		velocity.y += 0.04 * t;
	} else {
		if (velocity.y < 0.0)
			velocity.y /= 2.0;
		velocity.y += 0.007;
	}

	// Rider steers us by however fast they're trying to move
	if (auto rider = passenger.lock()) {
		velocity.x += rider->velocity.x * 0.2;
		velocity.z += rider->velocity.z * 0.2;
	}

	constexpr double MAX_HORIZONTAL_SPEED = 0.4;
	velocity.x = std::clamp(velocity.x, -MAX_HORIZONTAL_SPEED, MAX_HORIZONTAL_SPEED);
	velocity.z = std::clamp(velocity.z, -MAX_HORIZONTAL_SPEED, MAX_HORIZONTAL_SPEED);

	if (onGround) {
		velocity.x *= 0.5;
		velocity.y *= 0.5;
		velocity.z *= 0.5;
	}

	Move(this->velocity);

	double speed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);

	if (collidedHorizontally && speed > 0.15) {
		DropAsItems();
	} else {
		velocity.x *= 0.99;
		velocity.y *= 0.95;
		velocity.z *= 0.99;
	}

	// Face the direction we're actually travelling in
	rotationPitch = 0.0f;
	double desiredYaw = double(rotationYaw);
	double dx = prevPos.x - position.x;
	double dz = prevPos.z - position.z;
	if (dx * dx + dz * dz > 0.001) {
		desiredYaw = std::atan2(dz, dx) * 180.0 / JavaMath::PI;
	}

	double yawDelta = desiredYaw - double(rotationYaw);
	while (yawDelta >= 180.0)
		yawDelta -= 360.0;
	while (yawDelta < -180.0)
		yawDelta += 360.0;
	yawDelta = std::clamp(yawDelta, -20.0, 20.0);
	rotationYaw = float(double(rotationYaw) + yawDelta);

	// Push away any other boats we're overlapping
	auto nearby = entityManager->GetEntitiesWithinAabbExcluding(collider.Expand(0.2, 0.0, 0.2), id);
	auto rider = passenger.lock();
	for (auto& other : nearby) {
		if (rider && other == rider.get())
			continue;
		if (other->type != EntityType::BOAT || !other->CanBePushed())
			continue;

		Vec2 delta = { other->position.x - position.x, other->position.z - position.z };
		double dist = MathHelper::AbsMax(delta.x, delta.y);
		if (dist < 0.01)
			continue;

		dist = std::sqrt(dist);
		delta = delta / dist;
		double forceScale = std::min(1.0 / dist, 1.0);
		delta = delta * forceScale * 0.05;

		velocity.x -= delta.x;
		velocity.z -= delta.y;
		other->velocity.x += delta.x;
		other->velocity.z += delta.y;
	}

	// Melt snow layers under all 4 corners of the boat
	for (int corner = 0; corner < 4; corner++) {
		int cx = MathHelper::FloorDouble(position.x + (double(corner % 2) - 0.5) * 0.8);
		int cy = MathHelper::FloorDouble(position.y);
		int cz = MathHelper::FloorDouble(position.z + (double(corner / 2) - 0.5) * 0.8);
		if (world->GetBlockId({ cx, cy, cz }) == BlockType::BLOCK_SNOW_LAYER)
			world->SetBlock({ cx, cy, cz }, BlockType::BLOCK_AIR);
	}

	if (rider && rider->isDead) {
		passenger.reset();
		rider->vehicle.reset();
	}
}