/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#pragma once

#include "entity.h"
#include "explosion.h"
#include "logger.h"
#include "gamerules.h"
#include "world/world.h"

struct TntEntity : public Entity {
	int fuse = 0;

	// Used when loading from NBT
	TntEntity() : Entity() {
		type = EntityType::LIT_TNT;
		fuse = 80;
		SetSize({ 0.98f, 0.98f });
		yOffset = height / 2.0f;
	}

	explicit TntEntity(Vec3 _pos) : TntEntity() {
		Teleport(_pos);

		// Small random horizontal nudge plus a hop
		float randomDirection = float(this->rand.NextDouble() * JavaMath::PI * 2.0);
		velocity.x = -MathHelper::Sin(randomDirection * JavaMath::PI / 180.0) * 0.02f;
		velocity.y = double(0.2f);
		velocity.z = -MathHelper::Cos(randomDirection * JavaMath::PI / 180.0) * 0.02f;
	}

	void Tick() override;
	std::optional<Tag> SerializeToNbt() override;
	void LoadFromNbt(Tag& _nbt) override;

	bool CanBeCollidedWith() override {
		return !isDead;
	}

	void Explode() {
		if (!world)
			return;
		if (!Gamerule::Get<bool>(Gamerule::Id::TntExplodes))
			return;
		world->DoExplosion(nullptr, this->position, /*power=*/4.0f, /*doFire=*/false);
	}
};