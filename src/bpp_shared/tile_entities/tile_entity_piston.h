/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once
#include "tile_entity_base.h"

struct TileEntityPistonMoving : TileEntity {
	BlockType storedBlock = BLOCK_AIR;
	uint8_t storedMeta = 0;
	int orientation = 0;
	bool extending = false;
	float progress = 0.0f;
	float lastProgress = 0.0f;

	TileEntityPistonMoving(Int3 _pPosition) : TileEntity(TileType::PISTON_MOVING, _pPosition) {
		canTick = true;
	};

	Tag Serialize() override;
	void Deserialize(const Tag& _tag) override;
	void Tick(WorldManager& _world) override;
	void InstantFinish(WorldManager& _world);
	void NudgePushedObjects(WorldManager& _world, float _progress, float _pushDist);
	CollisionShape GetCollider(float _progress);
};