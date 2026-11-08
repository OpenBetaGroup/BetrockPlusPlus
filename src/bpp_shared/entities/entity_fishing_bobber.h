/*
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once
#include "entity.h"
#include <memory>

struct PlayerEntity;

struct FishingBobberEntity : public Entity {
	std::weak_ptr<PlayerEntity> owner;
	std::weak_ptr<Entity> hookedEntity;
	int catchableTicks = 0;

	FishingBobberEntity(std::shared_ptr<PlayerEntity> _owner, Vec3 _direction);

	void Tick() override;
	bool CanBeCollidedWith() override {
		return false;
	}
	bool IsCatchable() const {
		return catchableTicks > 0;
	}
	bool IsInGround() const {
		return inGround;
	}

private:
	Int3 tilePosition = { -1, -1, -1 };
	BlockType inTile = BLOCK_AIR;
	uint8_t tileMeta = 0;
	int ticksInAir = 0;
	int ticksInGround = 0;
	bool inGround = false;
};
