/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */

#include "entity_throwable.h"
#include "blocks/block_behaviors.h"
#include "damage_types.h"
#include "entity_manager.h"
#include "entity_mobile.h"
#include "helpers/aabb.h"
#include "raycast.h"
#include "world.h"
#include <string>
#include <unordered_map>
#include <vector>

static Tag SetShort(std::string _name, int16_t _value) {
	Tag shortTag;
	shortTag.type = TAG_SHORT;
	shortTag.name = _name;
	shortTag.shortValue = _value;

	return shortTag;
}

static Tag SetByte(std::string _name, int8_t _value) {
	Tag byteTag;
	byteTag.type = TAG_BYTE;
	byteTag.name = _name;
	byteTag.byteValue = _value;

	return byteTag;
}

std::optional<Tag> ThrowableEntity::SerializeToNbt() {
	auto tag = Entity::SerializeToNbt();
	if (!tag)
		return std::nullopt;

	tag->compound["xTile"] = SetShort("xTile", this->tilePosition.x);
	tag->compound["yTile"] = SetShort("yTile", this->tilePosition.y);
	tag->compound["zTile"] = SetShort("zTile", this->tilePosition.z);
	tag->compound["inTile"] = SetByte("inTile", this->inTile);
	tag->compound["inData"] = SetByte("inData", this->tileMeta);
	tag->compound["shake"] = SetByte("shake", this->shake);
	tag->compound["inGround"] = SetByte("inGround", this->throwableInGround);

	return tag;
}

void ThrowableEntity::LoadFromNbt(Tag& _nbt) {
	Entity::LoadFromNbt(_nbt);

	this->tilePosition.x = _nbt.Has("xTile") ? _nbt.Get("xTile").shortValue : -1;
	this->tilePosition.y = _nbt.Has("yTile") ? _nbt.Get("yTile").shortValue : -1;
	this->tilePosition.z = _nbt.Has("zTile") ? _nbt.Get("zTile").shortValue : -1;
	this->inTile = _nbt.Has("inTile") ? BlockType(_nbt.Get("inTile").byteValue) : BLOCK_AIR;
	this->tileMeta = _nbt.Has("inData") ? _nbt.Get("inData").byteValue : 0;
	this->shake = _nbt.Has("shake") ? _nbt.Get("shake").byteValue : 0;
	this->throwableInGround = _nbt.Has("inGround") ? _nbt.Get("inGround").byteValue : 0;
}

void ThrowableEntity::Tick() {
	this->prevRotationYaw = this->rotationYaw;
	this->prevRotationPitch = this->rotationPitch;

	Entity::Tick();

	// If we are constructed without a rotation then apply one
	if (this->prevRotationPitch == 0.0f && this->prevRotationYaw == 0.0f) {
		this->SetRotationBasedOnVelocity(this->velocity);
		this->prevRotationYaw = this->rotationYaw;
		this->prevRotationPitch = this->rotationPitch;
	}

	// Check if we are in a block
	BlockType currentTile = world->GetBlockId(this->tilePosition);
	if (currentTile > BLOCK_AIR) {
		auto bounds = Blocks::blockBehaviors[currentTile].getCollider(world->GetMetadata(this->tilePosition)).GetBounds();
		if (bounds && bounds->Offset(this->tilePosition.x, this->tilePosition.y, this->tilePosition.z)
		                  .PointIntersects(this->position))
			this->throwableInGround = true;
	}

	if (this->shake > 0) {
		this->shake--;
	}

	if (this->throwableInGround) {
		BlockType block = world->GetBlockId(this->tilePosition);
		uint8_t meta = world->GetMetadata(this->tilePosition);
		if (block == this->inTile && meta == this->tileMeta) {
			// Despawn after a minute of sitting in the ground
			this->ticksInGround++;
			if (this->ticksInGround == 1200)
				this->isDead = true;
		} else {
			// The block we were stuck in changed, so fall out of it
			this->throwableInGround = false;
			this->velocity.x *= double(this->rand.NextFloat() * 0.2f);
			this->velocity.y *= double(this->rand.NextFloat() * 0.2f);
			this->velocity.z *= double(this->rand.NextFloat() * 0.2f);
			this->ticksInGround = 0;
			this->ticksInAir = 0;
		}
		return;
	}

	this->ticksInAir++;

	// Did we hit a block?
	Vec3 start = this->position;
	Vec3 end = { this->position.x + this->velocity.x, this->position.y + this->velocity.y,
		         this->position.z + this->velocity.z };
	RayCastResult blockHit = Raycast::Raycast(*world, start, end, RayCastMode::IGNORE_FLUIDS,
	                                          /*_ignoreNonCollidable=*/true);
	if (blockHit.hit)
		end = blockHit.hitPosition;

	// Then against entities, only up to the block we hit
	auto ownerPtr = this->owner.lock();
	Entity* hitEntity = nullptr;
	double closestDistance = 0.0;
	AABB searchBox = this->collider.AddCoord(this->velocity.x, this->velocity.y, this->velocity.z).Expand(1.0, 1.0, 1.0);
	for (const auto& candidate : entityManager->GetEntitiesWithinAabbExcluding(searchBox, this->id)) {
		if (!candidate->CanBeCollidedWith())
			continue;

		// Don't shoot ourselves on the way out
		if (ownerPtr && candidate == ownerPtr.get() && this->ticksInAir < 5)
			continue;

		const double grow = 0.3;
		auto intercept = candidate->collider.Expand(grow, grow, grow).CalculateIntercept(start, end);
		if (!intercept)
			continue;

		double distance = start.Distance(intercept->point);
		if (distance < closestDistance || closestDistance == 0.0) {
			hitEntity = candidate;
			closestDistance = distance;
		}
	}

	if (blockHit.hit || hitEntity) {
		if (hitEntity) {
			// We dont do any damage but this will trigger the entity to be "irritated" at you
			hitEntity->AttackEntityFrom(ownerPtr.get(), 0, DamageType::None);
		}

		this->OnHit(this->position);

		this->isDead = true;
	}

	// Move
	this->position.x += this->velocity.x;
	this->position.y += this->velocity.y;
	this->position.z += this->velocity.z;

	// Ease our rotation towards our direction of travel
	this->SetRotationBasedOnVelocity(this->velocity);

	while (this->rotationPitch - this->prevRotationPitch < -180.0f)
		this->prevRotationPitch -= 360.0f;
	while (this->rotationPitch - this->prevRotationPitch >= 180.0f)
		this->prevRotationPitch += 360.0f;
	while (this->rotationYaw - this->prevRotationYaw < -180.0f)
		this->prevRotationYaw -= 360.0f;
	while (this->rotationYaw - this->prevRotationYaw >= 180.0f)
		this->prevRotationYaw += 360.0f;

	// Easing
	this->rotationPitch = this->prevRotationPitch + (this->rotationPitch - this->prevRotationPitch) * 0.2f;
	this->rotationYaw = this->prevRotationYaw + (this->rotationYaw - this->prevRotationYaw) * 0.2f;

	// Drag and gravity
	float drag = 0.99f;
	const float throwableGravity = 0.03f;
	if (this->inWater)
		drag = 0.8f;

	this->velocity.x *= double(drag);
	this->velocity.y *= double(drag);
	this->velocity.z *= double(drag);
	this->velocity.y -= double(throwableGravity);
	this->RebuildCollider();
}