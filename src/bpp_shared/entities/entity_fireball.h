/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#pragma once
#include "entity_mobile.h"
#include "logger.h"

struct FireballEntity : public Entity {
	Vec3 acceleration = {};

	// Default
	FireballEntity() : Entity() {
		this->InitFireball();
	}

	// Spawn from position and acceleration
	FireballEntity(Vec3 _position, Vec3 _acceleration) : Entity() {
		this->InitFireball();
		this->Teleport(_position);
		this->acceleration = this->NormalizeAcceleration(_acceleration);
	}

	// Spawn from entity
	FireballEntity(std::shared_ptr<MobileEntity> _owner, Vec3 _acceleration) : Entity() {
		this->InitFireball();
		if (!_owner) {
			Logger::Error << "Spawned throwable with invalid entity pointer!\n";
			return;
		}
		this->owner = _owner;
		this->Teleport(_owner->position, { _owner->rotationYaw, _owner->rotationPitch});
		this->velocity = {};
		_acceleration.x += rand.NextGaussian() * 0.4;
		_acceleration.y += rand.NextGaussian() * 0.4;
		_acceleration.z += rand.NextGaussian() * 0.4;
		this->acceleration = this->NormalizeAcceleration(_acceleration);
	}

	std::shared_ptr<MobileEntity> GetOwner() const {
		return owner.lock();
	}

	void Tick() override;

	bool CanBeCollidedWith() override {
		return true;
	}
private:
	// These are leftovers we serialize / deserialize from entity arrow
	// For vanilla save compat!!
	Int3 tilePosition = { -1, -1, -1 };
	BlockType inTile = BLOCK_AIR;
	int ticksInAir = 0;
	int ticksInGround = 0;
	int shake = 0;

	// These are used
	std::weak_ptr<MobileEntity> owner;
	float prevRotationYaw = 0.0f;
	float prevRotationPitch = 0.0f;

	bool throwableInGround : 1 = false;

	void InitFireball() {
		this->type = EntityType::FIREBALL;
		this->yOffset = 0.0f;
		this->SetSize({ 1.0, 1.0 });
	}

	Vec3 NormalizeAcceleration(Vec3 _acceleration) {
		auto length = _acceleration.Length();
		if (length == 0.0)
			return {};
		return (_acceleration / length) * 0.1;
	}

	void SetRotationBasedOnVelocity(Vec3 _velocity) {
		float horizontalMagnitude = MathHelper::SqrtDouble(_velocity.x * _velocity.x + _velocity.z * _velocity.z);
		this->rotationYaw = float(std::atan2(_velocity.x, _velocity.z) * 180.0 / JavaMath::PI);
		this->rotationPitch = float(std::atan2(_velocity.y, double(horizontalMagnitude)) * 180.0 / JavaMath::PI);
	}

	bool AttackEntityFrom(Entity* _entity, int /*_damage*/, DamageType /*_type*/) override {
		this->forceVelocityUpdate = true;
		
		if (_entity) {
			auto lookVec = _entity->GetLookVector(_entity->rotationYaw, _entity->rotationPitch);
			this->velocity = lookVec;
			this->acceleration = velocity * 0.1;
			return true;
		}
		return false;
	}
};