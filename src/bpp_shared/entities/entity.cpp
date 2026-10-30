/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#include "entity.h"
#include "entity_item.h"
#include "entity_manager.h"
#include "entity_player.h"
#include "entity_lightning_bolt.h"
#include "packet_data.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>

void Entity::OnMountEntity() {
	// stub
	return;
}

void Entity::OnDismountEntity() {
	// stub
	return;
}

void Entity::OnPlayerInteract(PlayerEntity* /*_entity*/) {
	// stub
	return;
}

void Entity::OnEnterPortal() {
	// stub
	return;
}

void Entity::OnStruckByLightning(LightningEntity& /*_bolt*/) {
	if (!isImmuneToFire)
		AttackEntityFrom(nullptr, 5, DamageType::Lightning);
	fireTicks++;
	if (fireTicks == 0)
		fireTicks = 300;
}

void Entity::DropItemAtEntity(ItemId _itemId, ItemAmount _count, ItemDamage _data, int _pickupTime) {
	Vec3 itemPos = position;
	std::shared_ptr<ItemEntity> itemEntity = std::make_shared<ItemEntity>(itemPos);
	itemEntity->itemStack = { _itemId, _count, _data };
	itemEntity->pickupCooldown = _pickupTime;

	// Register our item with the world
	this->world->entityManager.AddEntity(std::move(itemEntity));
}

bool Entity::PushOutOfBlocks(Vec3 _pos) {
	int bx = MathHelper::FloorDouble(_pos.x);
	int by = MathHelper::FloorDouble(_pos.y);
	int bz = MathHelper::FloorDouble(_pos.z);
	double fracX = _pos.x - double(bx);
	double fracY = _pos.y - double(by);
	double fracZ = _pos.z - double(bz);

	if (world->IsBlockNormalCube({ bx, by, bz })) {
		bool openNegX = !world->IsBlockNormalCube({ bx - 1, by, bz });
		bool openPosX = !world->IsBlockNormalCube({ bx + 1, by, bz });
		bool openNegY = !world->IsBlockNormalCube({ bx, by - 1, bz });
		bool openPosY = !world->IsBlockNormalCube({ bx, by + 1, bz });
		bool openNegZ = !world->IsBlockNormalCube({ bx, by, bz - 1 });
		bool openPosZ = !world->IsBlockNormalCube({ bx, by, bz + 1 });

		int8_t direction = -1;
		double closest = 9999.0;

		if (openNegX && fracX < closest) {
			closest = fracX;
			direction = 0;
		}
		if (openPosX && 1.0 - fracX < closest) {
			closest = 1.0 - fracX;
			direction = 1;
		}
		if (openNegY && fracY < closest) {
			closest = fracY;
			direction = 2;
		}
		if (openPosY && 1.0 - fracY < closest) {
			closest = 1.0 - fracY;
			direction = 3;
		}
		if (openNegZ && fracZ < closest) {
			closest = fracZ;
			direction = 4;
		}
		if (openPosZ && 1.0 - fracZ < closest) {
			closest = 1.0 - fracZ;
			direction = 5;
		}

		float pushSpeed = rand.NextFloat() * 0.2f + 0.1f;
		if (direction == 0)
			velocity.x = double(-pushSpeed);
		if (direction == 1)
			velocity.x = double(pushSpeed);
		if (direction == 2)
			velocity.y = double(-pushSpeed);
		if (direction == 3)
			velocity.y = double(pushSpeed);
		if (direction == 4)
			velocity.z = double(-pushSpeed);
		if (direction == 5)
			velocity.z = double(pushSpeed);
	}

	return false;
}
void Entity::OnCollideWithPlayer(PlayerEntity& /*_entity*/) {
	return;
}

float Entity::GetEntityBrightnessValue() {
	auto fd = MathHelper::FloorDouble;
	int x = fd(position.x);
	double offset = (collider.maxY - collider.minY) * 0.66;
	int y = fd(position.y - double(this->yOffset) + offset);
	int z = fd(position.z);
	if (this->world->AABBinValidChunks({ double(fd(collider.minX)), double(fd(collider.minY)),
	                                     double(fd(collider.minZ)), double(fd(collider.maxX)),
	                                     double(fd(collider.maxY)), double(fd(collider.maxZ)) })) {
		float brightness = Lighting::BrightnessArray[this->world->GetBlockLightValue({ x, y, z })];
		if (brightness < this->entityBrightness) {
			brightness = this->entityBrightness;
		}

		return brightness;
	}
	return this->entityBrightness;
}

void Entity::UpdateEntityPhysicsState() {
	// Returns if we are in water and applies a push to our entity
	if (world->HandleFluidAcceleration(GetFluidCollider(), Material::Water(), *this)) {
		fallDistance = 0.0;
		inWater = true;
		fireTicks = 0;
	} else {
		inWater = false;
	}

	if (fireTicks > 0) {
		if (isImmuneToFire) {
			fireTicks = std::max(0, fireTicks - 4);
		} else {
			if (fireTicks % 20 == 0)
				AttackEntityFrom(nullptr, 1, DamageType::Burn);
			fireTicks--;
		}
	}

	// Returns if we are in lava
	inLava = world->IsMaterialInAabb(GetLavaCollider(), Material::Lava());
	if (inLava && !isImmuneToFire) {
		AttackEntityFrom(nullptr, 4, DamageType::Lava);
		fireTicks = 600;
	}

	const bool wet = IsWet();

	bool inFire = inLava || world->IsMaterialInAabb(GetFireCollider(), Material::Fire());
	if (inFire) {
		if (!isImmuneToFire)
			AttackEntityFrom(nullptr, 1, DamageType::Burn); // dealFireDamage
		if (!wet && !isImmuneToFire) {
			// Count up through the grace period; catch fire when it reaches 0
			// While already burning this cancels the decrement above, so fire keeps us lit
			fireTicks++;
			if (fireTicks == 0)
				fireTicks = 300;
		}
	} else if (fireTicks <= 0) {
		// Out of fire and not burning
		fireTicks = -fireResistance;
	}

	// Rain or water puts us out and starts a fresh grace period
	if (wet && fireTicks > 0)
		fireTicks = -fireResistance;

	// Kill our entity if its below the world
	if (position.y < -64.0 && (type != EntityType::PLAYER))
		this->isDead = true;

	isFirstUpdate = false;
	UpdateMetadata(flags.isBurning, fireTicks > 0);
}

bool Entity::IsWet() {
	auto fd = MathHelper::FloorDouble;
	return inWater || world->CanRainHitSpot({ fd(position.x), fd(position.y), fd(position.z) });
}

void Entity::MountEntity(std::shared_ptr<Entity> _entity) {
	if (!entityManager) {
		Logger::Warning << "Tried to mount an entity before being initialized!\n";
		return;
	}
	auto entityPtr = _entity.get();
	if (entityPtr && entityPtr != this->vehicle.lock().get()) {
		UnmountEntity();
		if (auto oldPassenger = _entity->passenger.lock())
			oldPassenger->vehicle.reset();

		_entity->passenger = entityManager->GetEntityByIdShared(this->id);
		this->vehicle = _entity;

		this->passengerLookDelta = { 0.0f, 0.0f };
		this->lastVehicleRotation = { _entity->rotationYaw, _entity->rotationPitch };
		this->OnMountEntity();
		return;
	}
	UnmountEntity();
}

void Entity::UnmountEntity() {
	auto lockVehicle = vehicle.lock();
	if (lockVehicle) {
		lockVehicle->passenger.reset();
		Vec3 unmountPos = lockVehicle->position;
		unmountPos.y = lockVehicle->collider.minY + lockVehicle->height;
		this->Teleport(unmountPos, { this->rotationYaw, this->rotationPitch });
	}
	this->OnDismountEntity();
	this->vehicle.reset();
}

void Entity::TickPassengerEntity() {
	auto lockVehicle = this->vehicle.lock();
	if (!lockVehicle || lockVehicle->isDead) {
		this->vehicle.reset();
		return;
	}

	this->velocity = {};
	this->UpdateEntityPhysicsState();

	Vec3 seatOffset = lockVehicle->GetRiderSeatOffset();
	Vec3 newPos = { lockVehicle->position.x + seatOffset.x,
		            lockVehicle->position.y + lockVehicle->GetMountOffset() + this->GetRidingYOffset(),
		            lockVehicle->position.z + seatOffset.z };

	// Look direction smoothing
	passengerLookDelta.x += lockVehicle->rotationYaw - lastVehicleRotation.x;
	passengerLookDelta.y += lockVehicle->rotationPitch - lastVehicleRotation.y;
	lastVehicleRotation = { lockVehicle->rotationYaw, lockVehicle->rotationPitch };

	// Wrap into -180, 180
	while (passengerLookDelta.x >= 180.0f)
		passengerLookDelta.x -= 360.0f;
	while (passengerLookDelta.x < -180.0f)
		passengerLookDelta.x += 360.0f;
	while (passengerLookDelta.y >= 180.0f)
		passengerLookDelta.y -= 360.0f;
	while (passengerLookDelta.y < -180.0f)
		passengerLookDelta.y += 360.0f;

	constexpr float MAX_LOOK_STEP_PER_TICK = 10.0f;
	float yawStep = std::clamp(passengerLookDelta.x * 0.5f, -MAX_LOOK_STEP_PER_TICK, MAX_LOOK_STEP_PER_TICK);
	float pitchStep = std::clamp(passengerLookDelta.y * 0.5f, -MAX_LOOK_STEP_PER_TICK, MAX_LOOK_STEP_PER_TICK);

	passengerLookDelta.x -= yawStep;
	passengerLookDelta.y -= pitchStep;
	rotationYaw += yawStep;
	rotationPitch += pitchStep;

	this->Teleport(newPos, { rotationYaw, rotationPitch });
}

void Entity::Tick() {
	ticksExisted++;

	if (this->vehicle.expired())
		this->vehicle.reset();

	if (this->passenger.expired())
		this->passenger.reset();

	// Are we riding something?
	auto lockVehicle = this->vehicle.lock();
	if (lockVehicle) {
		this->TickPassengerEntity();
		return;
	}

	UpdateEntityPhysicsState();
}

void Entity::ApplyKnockback(Vec3 _direction) {
	velocity.x *= KNOCKBACK_VELOCITY_DAMPENING;
	velocity.y *= KNOCKBACK_VELOCITY_DAMPENING;
	velocity.z *= KNOCKBACK_VELOCITY_DAMPENING;
	velocity.x -= _direction.x * HORIZONTAL_KNOCKBACK;
	velocity.z -= _direction.z * HORIZONTAL_KNOCKBACK;
	velocity.y = std::min(float(velocity.y + VERTICAL_KNOCKBACK), VERTICAL_KNOCKBACK);
	forceVelocityUpdate = true;
}

void Entity::ApplyInput(float _acceleration) {
	float strafe = input.x;
	float forward = input.y;

	float length = MathHelper::SqrtFloat(strafe * strafe + forward * forward);
	if (length < 0.01f)
		return;

	if (length < 1.0f)
		length = 1.0f;

	length = _acceleration / length;
	strafe *= length;
	forward *= length;

	float sinYaw = MathHelper::Sin(rotationYaw * JavaMath::PI_FLOAT / 180.0f);
	float cosYaw = MathHelper::Cos(rotationYaw * JavaMath::PI_FLOAT / 180.0f);

	velocity.x += double(strafe * cosYaw - forward * sinYaw);
	velocity.z += double(forward * cosYaw + strafe * sinYaw);
}

void Entity::Move(Vec3& _velocity) {
	// Never simulate garbage. NaN would spin forever in the sneak-clamp loops below
	// (NaN != 0 and neither branch changes it), and a huge velocity makes the collision
	// query sweep a gigantic area. No legitimate entity moves anywhere near 128 blocks/tick.
	static constexpr double K_MAX_STEP = 128.0;
	auto sanitize = [](double _v) { return std::isfinite(_v) ? std::clamp(_v, -K_MAX_STEP, K_MAX_STEP) : 0.0; };
	_velocity.x = sanitize(_velocity.x);
	_velocity.y = sanitize(_velocity.y);
	_velocity.z = sanitize(_velocity.z);

	ySize *= 0.4f;

	bool clearMotionAfterMove = false;
	if (inWeb) {
		inWeb = false;
		_velocity.x *= COBWEB_HORIZONTAL_DRAG;
		_velocity.y *= COBWEB_VERTICAL_DRAG;
		_velocity.z *= COBWEB_HORIZONTAL_DRAG;
		clearMotionAfterMove = true;
	}

	Vec3 original = _velocity;
	AABB originalCollider = collider;
	bool clampSneak = onGround && flags.isSneaking;

	if (clampSneak) {
		const double step = 0.05;

		auto groundBelow = [&](double _dx, double _dz) -> bool {
			return !world->GetCollidingBoundingBoxes(collider.Offset(_dx, -1.0, _dz), this).empty();
		};

		// Clamp on the X and Z axes to avoid falling off edges while sneaking
		while (_velocity.x != 0.0 && !groundBelow(_velocity.x, 0.0)) {
			if (_velocity.x < step && _velocity.x >= -step)
				_velocity.x = 0.0;
			else if (_velocity.x > 0.0)
				_velocity.x -= step;
			else
				_velocity.x += step;
		}
		while (_velocity.z != 0.0 && !groundBelow(0.0, _velocity.z)) {
			if (_velocity.z < step && _velocity.z >= -step)
				_velocity.z = 0.0;
			else if (_velocity.z > 0.0)
				_velocity.z -= step;
			else
				_velocity.z += step;
		}

		// Update our og values so step up logic uses the correct position
		original.x = _velocity.x;
		original.z = _velocity.z;
	}

	auto sweptCollider = world->GetCollidingBoundingBoxes(collider.AddCoord(_velocity.x, _velocity.y, _velocity.z),
	                                                      this);

	// Resolve Y first
	for (auto& col : sweptCollider) {
		_velocity.y = col.CalculateYOffset(collider, _velocity.y);
	}
	collider = collider.Offset(0.0, _velocity.y, 0.0);

	// Check if we are on ground or landed this Tick
	bool canStepUp = onGround || (original.y != _velocity.y && original.y < 0.0);

	// Resolve X
	for (auto& col : sweptCollider) {
		_velocity.x = col.CalculateXOffset(collider, _velocity.x);
	}
	collider = collider.Offset(_velocity.x, 0.0, 0.0);

	// Resolve Z
	for (auto& col : sweptCollider) {
		_velocity.z = col.CalculateZOffset(collider, _velocity.z);
	}
	collider = collider.Offset(0.0, 0.0, _velocity.z);

	collidedHorizontally = original.x != _velocity.x || original.z != _velocity.z;

	if (stepHeight > 0.0f && canStepUp && (clampSneak || ySize < 0.05f) && collidedHorizontally) {
		Vec3 stepUpMovement = _velocity;
		_velocity = { original.x, stepHeight, original.z };

		AABB resolvedCollider = collider;
		collider = originalCollider;

		auto stepUpSweptCollider = world->GetCollidingBoundingBoxes(
		    collider.AddCoord(_velocity.x, _velocity.y, _velocity.z), this);

		// Resolve Y first
		for (auto& col : stepUpSweptCollider) {
			_velocity.y = col.CalculateYOffset(collider, _velocity.y);
		}
		collider = collider.Offset(0.0, _velocity.y, 0.0);

		// Resolve X
		for (auto& col : stepUpSweptCollider) {
			_velocity.x = col.CalculateXOffset(collider, _velocity.x);
		}
		collider = collider.Offset(_velocity.x, 0.0, 0.0);

		// Resolve Z
		for (auto& col : stepUpSweptCollider) {
			_velocity.z = col.CalculateZOffset(collider, _velocity.z);
		}
		collider = collider.Offset(0.0, 0.0, _velocity.z);

		// Snap down
		double downY = -stepHeight;
		for (auto& col : stepUpSweptCollider) {
			downY = col.CalculateYOffset(collider, downY);
		}
		collider = collider.Offset(0.0, downY, 0.0);

		// Keep whichever collision path moved further horizontally
		if (stepUpMovement.x * stepUpMovement.x + stepUpMovement.z * stepUpMovement.z >=
		    _velocity.x * _velocity.x + _velocity.z * _velocity.z) {
			_velocity = stepUpMovement;
			collider = resolvedCollider;
		} else {
			double frac = collider.minY - std::trunc(collider.minY);
			if (frac > 0.0)
				ySize += float(frac + 0.01);
		}
	}

	// Derive our current position from our collider
	position.x = (collider.minX + collider.maxX) / 2.0;
	position.y = collider.minY + double(yOffset) - double(ySize);
	position.z = (collider.minZ + collider.maxZ) / 2.0;

	collidedHorizontally = original.x != _velocity.x || original.z != _velocity.z;
	collidedVertically = original.y != _velocity.y;
	onGround = original.y != _velocity.y && original.y < 0.0;
	collided = collidedHorizontally || collidedVertically;

	if (original.x != _velocity.x)
		_velocity.x = 0.0;
	if (original.y != _velocity.y)
		_velocity.y = 0.0;
	if (original.z != _velocity.z)
		_velocity.z = 0.0;

	if (clearMotionAfterMove)
		velocity = {};

	UpdateFallState(_velocity.y);

	// Scan each block this entity overlaps so we can trigger collided with code
	auto minX = MathHelper::FloorDouble(collider.minX + 0.001);
	auto minY = MathHelper::FloorDouble(collider.minY + 0.001);
	auto minZ = MathHelper::FloorDouble(collider.minZ + 0.001);
	auto maxX = MathHelper::FloorDouble(collider.maxX - 0.001);
	auto maxY = MathHelper::FloorDouble(collider.maxY - 0.001);
	auto maxZ = MathHelper::FloorDouble(collider.maxZ - 0.001);
	if (world->AABBinValidChunks(
	        { double(minX), double(minY), double(minZ), double(maxX), double(maxY), double(maxZ) })) {
		for (int x = minX; x <= maxX; x++) {
			for (int y = minY; y <= maxY; y++) {
				for (int z = minZ; z <= maxZ; z++) {
					auto blockId = world->GetBlockId({ x, y, z });
					auto function = Blocks::blockBehaviors[blockId < 0 ? 0 : blockId].onEntityCollidedWithBlock;
					if (blockId > 0 && function) {
						function(*world, { x, y, z }, *this);
					}
				}
			}
		}
	}
}

void Entity::UpdateFallState(float _movedY) {
	if (onGround) {
		if (fallDistance > FALL_DAMAGE_FLOOR) {
			AttackEntityFrom(nullptr, int(std::ceil(fallDistance - FALL_DAMAGE_FLOOR)), DamageType::Fall);
		}
		fallDistance = 0;
	} else if (_movedY < 0) {
		fallDistance -= _movedY;
	}
}

void Entity::LoadFromNbt(Tag& _nbt) {
	if (_nbt.Has("Motion")) {
		auto& motion = _nbt.compound["Motion"].GetList();
		if (motion.size() >= 3) {
			velocity.x = motion[0].GetDouble();
			velocity.y = motion[1].GetDouble();
			velocity.z = motion[2].GetDouble();
		}
	}

	if (_nbt.Has("Pos")) {
		auto& pos = _nbt.compound["Pos"].GetList();
		if (pos.size() >= 3) {
			position.x = pos[0].GetDouble();
			position.y = pos[1].GetDouble();
			position.z = pos[2].GetDouble();
		}
	}

	if (_nbt.Has("Rotation")) {
		auto& rotation = _nbt.compound["Rotation"].GetList();
		if (rotation.size() >= 2) {
			rotationYaw = rotation[0].GetFloat();
			rotationPitch = rotation[1].GetFloat();
		}
	}

	air = _nbt.Has("Air") ? _nbt.compound["Air"].GetShort() : maxAir;
	onGround = _nbt.Has("OnGround") ? (_nbt.compound["OnGround"].GetByte() != 0) : true;
	fallDistance = _nbt.Has("FallDistance") ? _nbt.compound["FallDistance"].GetFloat() : 0.0f;
	fireTicks = _nbt.Has("Fire") ? _nbt.compound["Fire"].GetShort() : 0;

	RebuildCollider();
}

std::optional<Tag> Entity::SerializeToNbt() {
	Tag rootTag;
	rootTag.type = TAG_COMPOUND;
	rootTag.name = "";

	Tag motionTag;
	motionTag.type = TAG_LIST;
	motionTag.name = "Motion";
	motionTag.listType = TAG_DOUBLE;
	Tag airTag;
	airTag.type = TAG_SHORT;
	airTag.name = "Air";
	airTag.shortValue = this->air;
	Tag onGroundTag;
	onGroundTag.type = TAG_BYTE;
	onGroundTag.name = "OnGround";
	onGroundTag.byteValue = this->onGround;
	Tag fallDistanceTag;
	fallDistanceTag.type = TAG_FLOAT;
	fallDistanceTag.name = "FallDistance";
	fallDistanceTag.floatValue = this->fallDistance;
	Tag posTag;
	posTag.type = TAG_LIST;
	posTag.name = "Pos";
	posTag.listType = TAG_DOUBLE;
	Tag rotationTag;
	rotationTag.type = TAG_LIST;
	rotationTag.name = "Rotation";
	rotationTag.listType = TAG_FLOAT;
	Tag fireTag;
	fireTag.type = TAG_SHORT;
	fireTag.name = "Fire";
	fireTag.shortValue = this->fireTicks;

	// Save position and rotation / velocity
	Tag posXTag;
	posXTag.type = TAG_DOUBLE;
	posXTag.doubleValue = this->position.x;
	Tag posYTag;
	posYTag.type = TAG_DOUBLE;
	posYTag.doubleValue = this->position.y;
	Tag posZTag;
	posZTag.type = TAG_DOUBLE;
	posZTag.doubleValue = this->position.z;
	posTag.list.push_back(posXTag);
	posTag.list.push_back(posYTag);
	posTag.list.push_back(posZTag);

	Tag movXTag;
	movXTag.type = TAG_DOUBLE;
	movXTag.doubleValue = this->velocity.x;
	Tag movYTag;
	movYTag.type = TAG_DOUBLE;
	movYTag.doubleValue = this->velocity.y;
	Tag movZTag;
	movZTag.type = TAG_DOUBLE;
	movZTag.doubleValue = this->velocity.z;
	motionTag.list.push_back(movXTag);
	motionTag.list.push_back(movYTag);
	motionTag.list.push_back(movZTag);

	Tag rotYawTag;
	rotYawTag.type = TAG_FLOAT;
	rotYawTag.floatValue = this->rotationYaw;
	Tag rotPitchTag;
	rotPitchTag.type = TAG_FLOAT;
	rotPitchTag.floatValue = this->rotationPitch;
	rotationTag.list.push_back(rotYawTag);
	rotationTag.list.push_back(rotPitchTag);

	// Get our string ID
	auto stringId = this->entityManager->GetEntityNbtId(type);

	// If we don't have a string id fail to save
	if (!stringId)
		return std::nullopt;

	Tag idTag;
	idTag.type = TAG_STRING;
	idTag.name = "id";
	idTag.stringValue = *stringId;

	// Link together our compound
	rootTag.compound["Pos"] = posTag;
	rootTag.compound["Motion"] = motionTag;
	rootTag.compound["Rotation"] = rotationTag;
	rootTag.compound["FallDistance"] = fallDistanceTag;
	rootTag.compound["Fire"] = fireTag;
	rootTag.compound["Air"] = airTag;
	rootTag.compound["OnGround"] = onGroundTag;
	rootTag.compound["id"] = idTag;

	return rootTag;
}

void Entity::EncodeMetadata(std::vector<PacketData::EntityMetadata::DataEntry>& _metadata) {
	_metadata.push_back(
	    { .type = PacketData::EntityMetadata::BYTE,
	      .index = 0,
	      .value = int8_t(int8_t(flags.isBurning) | int8_t(flags.isSneaking) << 1 | int8_t(flags.isRiding) << 2) });
}

bool Entity::DecodeMetadata(const std::vector<PacketData::EntityMetadata::DataEntry>& _metadata) {
	// No metadata, give up
	if (_metadata.empty())
		return false;
	// TODO: Simplify this
	if (auto* raw = FindMetadata<int8_t>(_metadata, PacketData::EntityMetadata::BYTE, 0)) {
		flags.isBurning = (*raw & 0x1) != 0;
		flags.isSneaking = (*raw & 0x2) != 0;
		flags.isRiding = (*raw & 0x4) != 0;
		return true;
	}
	return false;
}