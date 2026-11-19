/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#pragma once
#include "blocks.h"
#include "entities.h"
#include "entity.h"
#include "entity_mobile.h"
#include "helpers/java/java_math.h"
#include "java_random.h"
#include "logger.h"
#include "logstream.h"
#include "nbt/nbt_tag.h"
#include "numeric_structs.h"
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>

struct ArrowEntity : public Entity {
	bool arrowBelongsToPlayer : 1 = false;

	// Default
	ArrowEntity() : Entity() {
		this->InitArrowEntity();
	}

	// Spawn from position
	ArrowEntity(Vec3 _position) : Entity() {
		this->InitArrowEntity();
		this->Teleport(_position);
	}

	// Spawn from entity
	ArrowEntity(std::shared_ptr<MobileEntity> _owner) : Entity() {
		this->InitArrowEntity();
		if (!_owner) {
			Logger::Error << "Spawned arrow with invalid entity pointer!\n";
			return;
		}
		this->owner = _owner;
		this->arrowBelongsToPlayer = _owner->type == EntityType::PLAYER;

		this->Teleport({ _owner->position.x, _owner->position.y + _owner->GetEyeHeight(), _owner->position.z },
		               { _owner->rotationYaw, _owner->rotationPitch });

		// Nudge the arrow sideways
		const float yawRad = this->rotationYaw / 180.0f * JavaMath::PI_FLOAT;
		const float pitchRad = this->rotationPitch / 180.0f * JavaMath::PI_FLOAT;
		this->position.x -= double(MathHelper::Cos(yawRad) * 0.16f);
		this->position.y -= 0.1;
		this->position.z -= double(MathHelper::Sin(yawRad) * 0.16f);
		this->RebuildCollider();

		// Initial direction is the shooter's look vector
		Vec3 heading = { double(-MathHelper::Sin(yawRad) * MathHelper::Cos(pitchRad)),
			             double(-MathHelper::Sin(pitchRad)),
			             double(MathHelper::Cos(yawRad) * MathHelper::Cos(pitchRad)) };
		this->SetArrowHeading(heading, /*strength=*/1.5f, /*inaccuracy=*/1.0f);
	}

	void SetArrowHeading(Vec3 _direction, float _strength, float _inaccuracy) {
		float magnitude = MathHelper::SqrtDouble(_direction.x * _direction.x + _direction.y * _direction.y +
		                                         _direction.z * _direction.z);
		_direction.x /= double(magnitude);
		_direction.y /= double(magnitude);
		_direction.z /= double(magnitude);

		_direction.x += rand.NextGaussian() * 0.007499999832361937 * double(_inaccuracy);
		_direction.y += rand.NextGaussian() * 0.007499999832361937 * double(_inaccuracy);
		_direction.z += rand.NextGaussian() * 0.007499999832361937 * double(_inaccuracy);

		_direction.x *= double(_strength);
		_direction.y *= double(_strength);
		_direction.z *= double(_strength);
		this->velocity = _direction;

		this->SetRotationBasedOnVelocity(this->velocity);
		this->prevRotationYaw = this->rotationYaw;
		this->prevRotationPitch = this->rotationPitch;
		this->ticksInGround = 0;
	}

	std::shared_ptr<MobileEntity> GetOwner() const {
		return owner.lock();
	}

	void Tick() override;
	void OnCollideWithPlayer(PlayerEntity& _entity) override;
	std::optional<Tag> SerializeToNbt() override;
	void LoadFromNbt(Tag& _nbt) override;

private:
	Int3 tilePosition = { -1, -1, -1 };
	BlockType inTile = BLOCK_AIR;
	uint8_t tileMeta = 0;
	int ticksInAir = 0;
	int ticksInGround = 0;
	int arrowShake = 0;
	std::weak_ptr<MobileEntity> owner;

	float prevRotationYaw = 0.0f;
	float prevRotationPitch = 0.0f;

	bool arrowInGround : 1 = false;

	void InitArrowEntity() {
		type = EntityType::ARROW;
		this->yOffset = 0.0f;
		this->SetSize({ 0.5, 0.5 });
	}

	void SetRotationBasedOnVelocity(Vec3 _velocity) {
		float horizontalMagnitude = MathHelper::SqrtDouble(_velocity.x * _velocity.x + _velocity.z * _velocity.z);
		this->rotationYaw = float(std::atan2(_velocity.x, _velocity.z) * 180.0 / JavaMath::PI);
		this->rotationPitch = float(std::atan2(_velocity.y, double(horizontalMagnitude)) * 180.0 / JavaMath::PI);
	}
};