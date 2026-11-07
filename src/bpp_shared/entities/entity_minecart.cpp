/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#include "entity_minecart.h"
#include "entity_manager.h"
#include "entity_mob.h"
#include "helpers/java/java_math.h"
#include "world/managers/rail_manager.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>

// This defines the start and end points along the rail blocks
inline constexpr int RAIL_MATRIX[10][2][3] = {
	{ { 0, 0, -1 }, { 0, 0, 1 } },  // 0: straight, along Z
	{ { -1, 0, 0 }, { 1, 0, 0 } },  // 1: straight, along X
	{ { -1, -1, 0 }, { 1, 0, 0 } }, // 2: slope, rises toward +X
	{ { -1, 0, 0 }, { 1, -1, 0 } }, // 3: slope, rises toward -X
	{ { 0, 0, -1 }, { 0, -1, 1 } }, // 4: slope, rises toward -Z
	{ { 0, -1, -1 }, { 0, 0, 1 } }, // 5: slope, rises toward +Z
	{ { 0, 0, 1 }, { 1, 0, 0 } },   // 6: curve, +Z <-> +X
	{ { 0, 0, 1 }, { -1, 0, 0 } },  // 7: curve, +Z <-> -X
	{ { 0, 0, -1 }, { -1, 0, 0 } }, // 8: curve, -Z <-> -X
	{ { 0, 0, -1 }, { 1, 0, 0 } },  // 9: curve, -Z <-> +X
};

void MinecartEntity::DropAsItems() {
	DropItemAtEntity(Items::MINECART, 1);
	isDead = true;
}

void MinecartEntity::ResolveEntityCollision(Entity& _other) {
	auto rider = passenger.lock();
	if (rider.get() == &_other)
		return;

	// Empty carts pick up mobs (not players)
	if (cartType == MinecartType::Empty && !rider && _other.vehicle.expired() &&
	    velocity.x * velocity.x + velocity.z * velocity.z > 0.01 && dynamic_cast<MobEntity*>(&_other)) {
		if (auto selfPtr = entityManager->GetEntityByIdShared(this->id))
			_other.MountEntity(selfPtr);
	}

	double dx = _other.position.x - position.x;
	double dz = _other.position.z - position.z;
	double distSq = dx * dx + dz * dz;
	if (distSq < double(1.0E-4f))
		return;

	double dist = double(float(std::sqrt(distSq)));
	dx /= dist;
	dz /= dist;

	double strength = std::min(1.0 / dist, 1.0);
	dx *= strength;
	dz *= strength;
	dx *= double(0.1f);
	dz *= double(0.1f);
	dx *= 0.5;
	dz *= 0.5;

	auto* otherCart = _other.type == EntityType::MINECART ? static_cast<MinecartEntity*>(&_other) : nullptr;
	if (!otherCart) {
		velocity.x -= dx;
		velocity.z -= dz;
		_other.velocity.x += dx / 4.0;
		_other.velocity.z += dz / 4.0;
		return;
	}

	double relX = _other.position.x - position.x;
	double relZ = _other.position.z - position.z;
	double approach = relX * otherCart->velocity.z + relZ * otherCart->prevPosition.x;
	if (approach * approach > 5.0)
		return;

	constexpr double KEEP = double(0.2f);
	constexpr double FURNACE_KEEP = double(0.7f);
	double sharedX = otherCart->velocity.x + velocity.x;
	double sharedZ = otherCart->velocity.z + velocity.z;
	bool otherIsFurnace = otherCart->cartType == MinecartType::Furnace;
	bool thisIsFurnace = cartType == MinecartType::Furnace;

	if (otherIsFurnace && !thisIsFurnace) {
		// A furnace cart shoves us along at its speed
		velocity.x *= KEEP;
		velocity.z *= KEEP;
		velocity.x += otherCart->velocity.x - dx;
		velocity.z += otherCart->velocity.z - dz;
		otherCart->velocity.x *= FURNACE_KEEP;
		otherCart->velocity.z *= FURNACE_KEEP;
	} else if (!otherIsFurnace && thisIsFurnace) {
		// We're the furnace cart, so we shove them
		otherCart->velocity.x *= KEEP;
		otherCart->velocity.z *= KEEP;
		otherCart->velocity.x += velocity.x + dx;
		otherCart->velocity.z += velocity.z + dz;
		velocity.x *= FURNACE_KEEP;
		velocity.z *= FURNACE_KEEP;
	} else {
		// Same kind: both end up moving at their average speed, nudged apart
		sharedX /= 2.0;
		sharedZ /= 2.0;
		velocity.x *= KEEP;
		velocity.z *= KEEP;
		velocity.x += sharedX - dx;
		velocity.z += sharedZ - dz;
		otherCart->velocity.x *= KEEP;
		otherCart->velocity.z *= KEEP;
		otherCart->velocity.x += sharedX + dx;
		otherCart->velocity.z += sharedZ + dz;
	}
}

void MinecartEntity::ResolveEntityPushes() {
	auto rider = passenger.lock();
	auto nearby = world->entityManager.GetEntitiesWithinAabbExcluding(collider.Expand(double(0.2f), 0.0, double(0.2f)),
	                                                                  id);

	for (Entity* other : nearby) {
		if (rider && other == rider.get())
			continue;
		if (other->type != EntityType::MINECART || !other->CanBePushed())
			continue;
		static_cast<MinecartEntity*>(other)->ResolveEntityCollision(*this);
	}
}

bool MinecartEntity::AttackEntityFrom(Entity* _entity, int _damage, DamageType _type) {
	if (isDead)
		return true;

	Entity::AttackEntityFrom(_entity, _damage, _type);

	shakeTimer = 10;
	damageTaken += _damage * 10;

	if (damageTaken > 40) {
		if (auto rider = passenger.lock())
			rider->UnmountEntity();
		DropAsItems();
	}

	return true;
}

void MinecartEntity::OnPlayerInteract(PlayerEntity* _entity) {
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

std::optional<Vec3> MinecartEntity::GetClosestPositionAlongRail(Vec3 _position) {
	auto fd = MathHelper::FloorDouble;
	Int3 blockPos = { fd(_position.x), fd(_position.y), fd(_position.z) };

	if (RailManager::IsRail(this->world->GetBlockId(blockPos.WithOffset(Cardinal::Value::Down)))) {
		blockPos.y--;
	}

	auto thisRail = this->world->GetBlockId(blockPos);
	if (!RailManager::IsRail(thisRail))
		return std::nullopt;

	// Use the shape stored in the metadata (powered/detector bit stripped)
	auto thisShape = RailManager::GetRailShape(this->world->GetMetadata(blockPos), thisRail);
	int8_t shapeAsByte = int8_t(thisShape);
	if (shapeAsByte < 0 || shapeAsByte > 9)
		return std::nullopt; // corrupt metadata, not a valid rail shape
	const auto& ends = RAIL_MATRIX[shapeAsByte];
	double ax = blockPos.x + 0.5 + ends[0][0] * 0.5;
	double ay = blockPos.y + 0.5 + ends[0][1] * 0.5;
	double az = blockPos.z + 0.5 + ends[0][2] * 0.5;
	double bxe = blockPos.x + 0.5 + ends[1][0] * 0.5;
	double bye = blockPos.y + 0.5 + ends[1][1] * 0.5;
	double bze = blockPos.z + 0.5 + ends[1][2] * 0.5;

	// Direction A to B. Y is doubled because the LUT stores slopes at half height
	double dx = bxe - ax;
	double dy = (bye - ay) * 2.0;
	double dz = bze - az;

	double t = 0.0;
	if (dx == 0.0) { // Runs along Z
		_position.x = blockPos.x + 0.5;
		t = _position.z - blockPos.z;
	} else if (dz == 0.0) { // Runs along X
		_position.z = blockPos.z + 0.5;
		t = _position.x - blockPos.x;
	} else { // Curved, so project onto line; |d|^2 = 0.5
		t = ((_position.x - ax) * dx + (_position.z - az) * dz) * 2.0;
	}

	_position.x = ax + dx * t;
	_position.y = ay + dy * t;
	_position.z = az + dz * t;

	// Normalise slope height so both directions give low end +0.5, high end +1.5
	if (dy < 0.0)
		_position.y += 1.0;
	if (dy > 0.0)
		_position.y += 0.5;

	return _position;
}

void MinecartEntity::UpdateYaw(const Vec3& _prevPos, float _prevYaw) {
	rotationPitch = 0.0f;

	// Face the way we moved
	double dx = _prevPos.x - position.x;
	double dz = _prevPos.z - position.z;
	if (dx * dx + dz * dz > 0.001) {
		rotationYaw = float(std::atan2(dz, dx) * 180.0 / JavaMath::PI);
		if (isInReverse)
			rotationYaw += 180.0f;
	}

	// A 180 degree flip means we reversed direction
	double delta = double(rotationYaw - _prevYaw);
	while (delta >= 180.0)
		delta -= 360.0;
	while (delta < -180.0)
		delta += 360.0;
	if (delta < -170.0 || delta >= 170.0) {
		rotationYaw += 180.0f;
		isInReverse = !isInReverse;
	}

	rotationYaw = std::fmod(rotationYaw, 360.0f);
	rotationPitch = std::fmod(rotationPitch, 360.0f);
}

void MinecartEntity::Tick() {
	if (shakeTimer > 0)
		shakeTimer--;

	if (damageTaken > 0)
		damageTaken--;

	Vec3 prevPos = position;
	prevPosition = position;
	float prevYaw = rotationYaw;

	this->velocity.y -= 0.04;

	auto fd = MathHelper::FloorDouble;
	Int3 blockPos = { fd(position.x), fd(position.y), fd(position.z) };

	if (RailManager::IsRail(this->world->GetBlockId(blockPos.WithOffset(Cardinal::Value::Down)))) {
		blockPos.y--;
	}

	const double MAX_VEL = 0.4;
	const double SLOPE_ACCEL = 0.0078125;
	const double RIDDEN_FRICTION = double(0.997f);
	const double EMPTY_FRICTION = double(0.96f);
	const double BOOST_ACCEL = 0.06;
	const double BOOST_KICK = 0.02;
	auto thisRail = this->world->GetBlockId(blockPos);

	if (RailManager::IsRail(thisRail)) {
		// Do fancy rail snapping here
		auto thisMeta = this->world->GetMetadata(blockPos);
		auto thisDirection = RailManager::GetRailShape(thisMeta, thisRail);

		// Powered rail behavior
		bool isPoweredRail = thisRail == BLOCK_RAIL_POWERED;
		bool boosting = isPoweredRail && (thisMeta & 8) != 0;
		bool braking = isPoweredRail && (thisMeta & 8) == 0;

		std::optional<Vec3> before = GetClosestPositionAlongRail(this->position);
		if (!before.has_value())
			return;

		this->position.y = blockPos.y;

		const auto& ends = RAIL_MATRIX[int8_t(thisDirection)];
		bool sloped = int8_t(thisDirection) >= 2 && int8_t(thisDirection) <= 5;

		// Slopes pull the cart toward their low end
		if (sloped) {
			const int* low = ends[0][1] < 0 ? ends[0] : ends[1];
			velocity.x += low[0] * SLOPE_ACCEL;
			velocity.z += low[2] * SLOPE_ACCEL;
		}

		// Redirect horizontal velocity along the rail, keeping speed and picking the closer direction
		double dirX = ends[1][0] - ends[0][0];
		double dirZ = ends[1][2] - ends[0][2];
		double dirLength = std::sqrt(dirX * dirX + dirZ * dirZ);
		if (velocity.x * dirX + velocity.z * dirZ < 0.0) {
			dirX = -dirX;
			dirZ = -dirZ;
		}
		double speed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
		velocity.x = speed * dirX / dirLength;
		velocity.z = speed * dirZ / dirLength;

		// Unpowered powered rails brake
		if (braking) {
			if (std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z) < 0.03) {
				velocity.x = 0.0;
				velocity.z = 0.0;
			} else {
				velocity.x *= 0.5;
				velocity.z *= 0.5;
			}
		}

		// Snap onto the rail
		position.x = before->x;
		position.y = double(blockPos.y + (sloped ? 1 : 0)) + double(yOffset);
		position.z = before->z;
		RebuildCollider();

		// Riders slow the step and each axis is capped
		Vec3 step = { velocity.x, 0.0, velocity.z };
		if (passenger.lock()) {
			step.x *= 0.75;
			step.z *= 0.75;
		}
		step.x = std::clamp(step.x, -MAX_VEL, MAX_VEL);
		step.z = std::clamp(step.z, -MAX_VEL, MAX_VEL);

		Vec3 requested = step;
		Move(step);
		if (step.x != requested.x)
			velocity.x = 0.0;
		if (step.z != requested.z)
			velocity.z = 0.0;

		// Leaving a slope through its low end so see if we can drop down a block
		for (const auto& end : ends) {
			if (end[1] != 0 && MathHelper::FloorDouble(position.x) - blockPos.x == end[0] &&
			    MathHelper::FloorDouble(position.z) - blockPos.z == end[2]) {
				position.y += end[1];
				RebuildCollider();
				break;
			}
		}

		// Friction
		if (passenger.lock()) {
			velocity.x *= RIDDEN_FRICTION;
			velocity.z *= RIDDEN_FRICTION;
		} else {
			velocity.x *= EMPTY_FRICTION;
			velocity.z *= EMPTY_FRICTION;
		}
		// On a rail Y velocity is always cleared
		velocity.y = 0.0;

		// Going downhill speeds the cart up, uphill slows it
		if (std::optional<Vec3> after = GetClosestPositionAlongRail(position)) {
			double gain = (before->y - after->y) * 0.05;
			double currentSpeed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
			if (currentSpeed > 0.0) {
				velocity.x = velocity.x / currentSpeed * (currentSpeed + gain);
				velocity.z = velocity.z / currentSpeed * (currentSpeed + gain);
			}
			position.y = after->y;
			RebuildCollider();
		}

		// Entered a new block so point velocity straight at it
		int newX = MathHelper::FloorDouble(position.x);
		int newZ = MathHelper::FloorDouble(position.z);
		if (newX != blockPos.x || newZ != blockPos.z) {
			double currentSpeed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
			velocity.x = currentSpeed * double(newX - blockPos.x);
			velocity.z = currentSpeed * double(newZ - blockPos.z);
		}

		// Powered rails boost, or kick a stopped cart away from a solid block on straight track
		if (boosting) {
			double currentSpeed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
			if (currentSpeed > 0.01) {
				velocity.x += velocity.x / currentSpeed * BOOST_ACCEL;
				velocity.z += velocity.z / currentSpeed * BOOST_ACCEL;
			} else if (thisDirection == Blocks::RailShape::FlatEastWest) {
				if (world->IsBlockNormalCube({ blockPos.x - 1, blockPos.y, blockPos.z }))
					velocity.x = BOOST_KICK;
				else if (world->IsBlockNormalCube({ blockPos.x + 1, blockPos.y, blockPos.z }))
					velocity.x = -BOOST_KICK;
			} else if (thisDirection == Blocks::RailShape::FlatNorthSouth) {
				if (world->IsBlockNormalCube({ blockPos.x, blockPos.y, blockPos.z - 1 }))
					velocity.z = BOOST_KICK;
				else if (world->IsBlockNormalCube({ blockPos.x, blockPos.y, blockPos.z + 1 }))
					velocity.z = -BOOST_KICK;
			}
		}
	} else {
		// This is our free fall logic
		this->velocity.x = std::clamp(this->velocity.x, -MAX_VEL, MAX_VEL);
		this->velocity.z = std::clamp(this->velocity.z, -MAX_VEL, MAX_VEL);

		if (this->onGround)
			this->velocity *= 0.5;

		this->Move(this->velocity);
		if (!this->onGround)
			this->velocity *= 0.95;
	}

	UpdateYaw(prevPos, prevYaw);
	ResolveEntityPushes();

	if (this->vehicle.expired())
		this->vehicle.reset();

	if (this->passenger.expired())
		this->passenger.reset();
}