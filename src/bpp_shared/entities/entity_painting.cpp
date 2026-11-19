/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#include "entity_painting.h"
#include "blocks/materials.h"
#include "direction_fixer.h"
#include "entity.h"
#include "entity_manager.h"
#include "helpers/aabb.h"
#include "helpers/java/java_math.h"
#include "packet_data.h"
#include "world/world.h"
#include <cstdint>
#include <unordered_map>

void PaintingEntity::SetDirection(Cardinal::Value _direction) {
	this->direction = _direction;
	float halfWidth = this->art.sizeX;
	float halfHeight = this->art.sizeY;
	float halfDepth = this->art.sizeX;

	if (_direction == Cardinal::Value::East || _direction == Cardinal::Value::West) {
		halfWidth = 0.5f;
	} else {
		halfDepth = 0.5f;
	}

	halfWidth /= 32.0f;
	halfHeight /= 32.0f;
	halfDepth /= 32.0f;

	Vec3 centerPos = { this->blockCoordinates.x + 0.5, this->blockCoordinates.y + 0.5, this->blockCoordinates.z + 0.5 };
	float wallOffset = 0.5625f;
	switch (_direction) {
	case Cardinal::Value::North:
		centerPos.z -= wallOffset;
		centerPos.x -= GetArtSize(art.sizeX);
		break;
	case Cardinal::Value::West:
		centerPos.x -= wallOffset;
		centerPos.z += GetArtSize(art.sizeX);
		break;
	case Cardinal::Value::South:
		centerPos.z += wallOffset;
		centerPos.x += GetArtSize(art.sizeX);
		break;
	case Cardinal::Value::East:
		centerPos.x += wallOffset;
		centerPos.z -= GetArtSize(art.sizeX);
		break;
	default:
		break;
	}

	centerPos.y += GetArtSize(art.sizeY);
	this->Teleport(centerPos);
	float epsilon = -0.00625f;
	this->collider = { double(centerPos.x - halfWidth - epsilon),  double(centerPos.y - halfHeight - epsilon),
		               double(centerPos.z - halfDepth - epsilon),  double(centerPos.x + halfWidth + epsilon),
		               double(centerPos.y + halfHeight + epsilon), double(centerPos.z + halfDepth + epsilon) };
}

bool PaintingEntity::OnValidSurface() {
	if (!world || !entityManager)
		return false;

	// Reject if our own hitbox already overlaps a solid block collider
	if (!world->GetCollidingBoundingBoxes(this->collider, this).empty())
		return false;

	int artWidthBlocks = art.sizeX / 16;
	int artHeightBlocks = art.sizeY / 16;

	Int3 blockPos = blockCoordinates;
	float halfArtWidth = float(art.sizeX) / 32.0f;

	if (direction == Cardinal::Value::North || direction == Cardinal::Value::South) {
		blockPos.x = MathHelper::FloorDouble(position.x - double(halfArtWidth));
	} else if (direction == Cardinal::Value::East || direction == Cardinal::Value::West) {
		blockPos.z = MathHelper::FloorDouble(position.z - double(halfArtWidth));
	}

	blockPos.y = MathHelper::FloorDouble(position.y - double(art.sizeY) / 32.0);

	for (int col = 0; col < artWidthBlocks; ++col) {
		for (int row = 0; row < artHeightBlocks; ++row) {
			Material material;
			if (direction == Cardinal::Value::East || direction == Cardinal::Value::West) {
				// wall runs along Z at the fixed anchor X
				material = world->GetMaterial({ blockCoordinates.x, blockPos.y + row, blockPos.z + col });
			} else {
				// wall runs along X at the fixed anchor Z
				material = world->GetMaterial({ blockPos.x + col, blockPos.y + row, blockCoordinates.z });
			}

			if (!material.isSolid)
				return false;
		}
	}

	// Reject if another painting is already hanging in this space
	auto nearby = entityManager->GetEntitiesWithinAabbExcluding(this->collider, this->id);
	for (auto& other : nearby) {
		if (other->type == EntityType::PAINTING)
			return false;
	}

	return true;
}

void PaintingEntity::LoadFromNbt(Tag& _nbt) {
	Entity::LoadFromNbt(_nbt);
	this->direction = _nbt.Has("Dir") ? FromPaintingDirectionToDirection(
	                                        static_cast<PacketData::PaintingDirection>(_nbt.compound["Dir"].GetByte()))
	                                  : Cardinal::Value::North;
	art = _nbt.Has("Motive") ? GetArtProfile(_nbt.compound["Motive"].stringValue) : GetArtProfile("Kebab");
	if (_nbt.Has("TileX") && _nbt.Has("TileY") && _nbt.Has("TileZ")) {
		this->blockCoordinates.x = _nbt.Get("TileX").intValue;
		this->blockCoordinates.y = _nbt.Get("TileY").intValue;
		this->blockCoordinates.z = _nbt.Get("TileZ").intValue;

		this->SetDirection(direction);
		return;
	}
	this->isDead = true;
}

std::optional<Tag> PaintingEntity::SerializeToNbt() {
	auto tag = Entity::SerializeToNbt();
	if (!tag)
		return std::nullopt;

	Tag dirTag;
	dirTag.type = TAG_BYTE;
	dirTag.name = "Dir";
	dirTag.byteValue = static_cast<int8_t>(FromDirectionToPaintingDirection(direction));

	Tag motiveTag;
	motiveTag.type = TAG_STRING;
	motiveTag.name = "Motive";
	motiveTag.stringValue = art.title;

	Tag tileX;
	tileX.type = TAG_INT;
	tileX.name = "TileX";
	tileX.intValue = blockCoordinates.x;

	Tag tileY;
	tileY.type = TAG_INT;
	tileY.name = "TileY";
	tileY.intValue = blockCoordinates.y;

	Tag tileZ;
	tileZ.type = TAG_INT;
	tileZ.name = "TileZ";
	tileZ.intValue = blockCoordinates.z;

	tag->compound["Dir"] = dirTag;
	tag->compound["TileX"] = tileX;
	tag->compound["TileY"] = tileY;
	tag->compound["TileZ"] = tileZ;
	tag->compound["Motive"] = motiveTag;

	return tag;
}