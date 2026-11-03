/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "tile_entity_piston.h"
#include "world.h"

static constexpr int PISTON_DX[6] = { 0, 0, 0, 0, -1, 1 };
static constexpr int PISTON_DY[6] = { -1, 1, 0, 0, 0, 0 };
static constexpr int PISTON_DZ[6] = { 0, 0, -1, 1, 0, 0 };

void TileEntityPistonMoving::Deserialize(const Tag& _tag) {
	if (_tag.Has("blockId"))
		this->storedBlock = BlockType(_tag.Get("blockId").GetInt());
	if (_tag.Has("blockData"))
		this->storedMeta = uint8_t(_tag.Get("blockData").GetInt());
	if (_tag.Has("facing"))
		this->orientation = _tag.Get("facing").GetInt();
	if (_tag.Has("progress"))
		this->lastProgress = this->progress = _tag.Get("progress").GetFloat();
	if (_tag.Has("extending"))
		this->extending = _tag.Get("extending").GetByte() != 0;
	if (this->orientation < 0 || this->orientation > 5)
		this->orientation = 0;
}

void TileEntityPistonMoving::Tick(WorldManager& _world) {
	this->lastProgress = this->progress;

	// We finished
	if (this->lastProgress >= 1.0f) {
		NudgePushedObjects(_world, 1.0f, 0.25f);
		_world.RemoveTileEntity(this->position);
		if (this->chunk)
			chunk->isModified = true;

		if (_world.GetBlockId(this->position) == BLOCK_PISTON_MOVING) {
			_world.SetBlock(this->position, this->storedBlock, this->storedMeta);
		}
		return;
	}

	// Still pushing
	this->progress += 0.5f;
	if (this->progress >= 1.0f)
		this->progress = 1.0f;

	if (this->extending)
		NudgePushedObjects(_world, this->progress, this->progress - this->lastProgress + 0.0625f);
}

CollisionShape TileEntityPistonMoving::GetCollider(float _progress) {
	if (storedBlock == BLOCK_AIR || storedBlock == BLOCK_PISTON_MOVING)
		return {};

	auto collider = Blocks::blockBehaviors[storedBlock].getCollider(storedMeta);

	return collider.Offset(position.x - PISTON_DX[orientation] * _progress,
	                       position.y - PISTON_DY[orientation] * _progress,
	                       position.z - PISTON_DZ[orientation] * _progress);
}

void TileEntityPistonMoving::NudgePushedObjects(WorldManager& _world, float _progress, float _pushDist) {
	if (!extending)
		_progress -= 1.0f;
	else
		_progress = 1.0f - _progress;

	auto movingBox = GetCollider(_progress);
	if (movingBox.IsEmpty())
		return;

	auto bounds = movingBox.GetBounds();
	if (!bounds)
		return;

	auto entities = _world.entityManager.GetEntitiesWithinAabb(*bounds);
	for (auto& pEntity : entities) {
		// We need a new copy each time since entities mutate this vector
		Vec3 pushVel = { PISTON_DX[orientation] * double(_pushDist), PISTON_DY[orientation] * double(_pushDist),
			             PISTON_DZ[orientation] * double(_pushDist) };
		pEntity->Move(pushVel);
	}
}

void TileEntityPistonMoving::InstantFinish(WorldManager& _world) {
	if (this->lastProgress >= 1.0f)
		return;

	this->lastProgress = this->progress = 1.0f;
	_world.RemoveTileEntity(this->position);

	if (this->chunk)
		chunk->isModified = true;

	if (_world.GetBlockId(this->position) == BLOCK_PISTON_MOVING) {
		_world.SetBlock(this->position, this->storedBlock, this->storedMeta);
	}
}

Tag TileEntityPistonMoving::Serialize() {
	auto root = TileEntity::Serialize();
	root.compound["blockId"] = Tag{ .type = TAG_INT, .name = "blockId", .intValue = int32_t(storedBlock) };
	root.compound["blockData"] = Tag{ .type = TAG_INT, .name = "blockData", .intValue = int32_t(storedMeta) };
	root.compound["facing"] = Tag{ .type = TAG_INT, .name = "facing", .intValue = int32_t(orientation) };
	// Java saves lastProgress
	root.compound["progress"] = Tag{ .type = TAG_FLOAT, .name = "progress", .floatValue = lastProgress };
	root.compound["extending"] = Tag{ .type = TAG_BYTE, .name = "extending", .byteValue = int8_t(extending ? 1 : 0) };
	return root;
}