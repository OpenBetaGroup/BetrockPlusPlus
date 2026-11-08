/*
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "entity_fishing_bobber.h"
#include "entity_manager.h"
#include "entity_mobile.h"
#include "entity_player.h"
#include "items.h"
#include "raycast.h"
#include "world/world.h"
#include <cmath>

FishingBobberEntity::FishingBobberEntity(std::shared_ptr<PlayerEntity> _owner, Vec3 _direction) {
	type = EntityType::FISHING_BOBBER;
	this->owner = _owner;
	SetSize({ 0.25f, 0.25f });

	if (!_owner)
		return;

	Teleport({ _owner->position.x, _owner->position.y + _owner->GetEyeHeight() - _owner->yOffset, _owner->position.z },
	         { _owner->rotationYaw, _owner->rotationPitch });

	const float yaw = rotationYaw * (JavaMath::PI_FLOAT / 180.0f);
	position.x -= double(std::cos(yaw) * 0.16f);
	position.y -= 0.1;
	position.z -= double(std::sin(yaw) * 0.16f);

	RebuildCollider();
	float length = _direction.Length();
	if (length == 0.0f)
		return;

	_direction.x /= length;
	_direction.y /= length;
	_direction.z /= length;
	// stole this spread from entity_arrow.h without proper investigation
	// feels like this should be a macro for this magic number. oh well!
	_direction.x += rand.NextGaussian() * 0.007499999832361937 * 1.0;
	_direction.y += rand.NextGaussian() * 0.007499999832361937 * 1.0;
	_direction.z += rand.NextGaussian() * 0.007499999832361937 * 1.0;

	velocity = { _direction.x * 1.5, _direction.y * 1.5, _direction.z * 1.5 };
}

void FishingBobberEntity::Tick() {
	Entity::Tick();

	if (isDead || !world || !entityManager)
		return;
	auto player = owner.lock();
	if (!player) {
		isDead = true;
		return;
	}

	auto heldStack = player->GetHeldItem();

	// detach if player has died/bad held item/moved too far
	if (player->isDead || !heldStack || heldStack->id != Items::Id::FISHING_ROD ||
	    player->position.DistanceSquared(position) > 1024.0) {
		if (player->fishingBobber.lock().get() == this)
			player->fishingBobber.reset();

		isDead = true;
		return;
	}

	// follow hooked entity
	if (auto hooked = hookedEntity.lock()) {
		if (!hooked->isDead) {
			position = { hooked->position.x, hooked->collider.minY + double(hooked->height) * 0.8, hooked->position.z };
			velocity = {};
			RebuildCollider();
			return;
		}
		hookedEntity.reset();
	}

	if (catchableTicks > 0)
		--catchableTicks;

	if (inGround) {
		if (world->GetBlockId(tilePosition) == inTile && world->GetMetadata(tilePosition) == tileMeta) {
			if (++ticksInGround >= 1200)
				isDead = true;
			return;
		}

		inGround = false;
		velocity.x *= double(rand.NextFloat() * 0.2f);
		velocity.y *= double(rand.NextFloat() * 0.2f);
		velocity.z *= double(rand.NextFloat() * 0.2f);
		ticksInGround = ticksInAir = 0;
	}
	++ticksInAir;

	Vec3 start = position;
	Vec3 end = { position.x + velocity.x, position.y + velocity.y, position.z + velocity.z };

	auto blockHit = Raycast::Raycast(*world, start, end, RayCastMode::IGNORE_FLUIDS, true);
	if (blockHit.hit)
		end = blockHit.hitPosition;

	Entity* hitEntity = nullptr;
	double closest = 0.0;

	// sweep entities in the bobber's path
	AABB search = collider.AddCoord(velocity.x, velocity.y, velocity.z).Expand(1.0, 1.0, 1.0);
	for (Entity* candidate : entityManager->GetEntitiesWithinAabbExcluding(search, id)) {
		if (!candidate->CanBeCollidedWith() || (candidate == player.get() && ticksInAir < 5))
			continue;

		auto intercept = candidate->collider.Expand(0.3, 0.3, 0.3).CalculateIntercept(start, end);
		if (!intercept)
			continue;

		double distance = start.Distance(intercept->point);
		if (closest == 0.0 || distance < closest) {
			hitEntity = candidate;
			closest = distance;
		}
	}

	if (hitEntity && (!blockHit.hit || closest < start.Distance(blockHit.hitPosition))) {
		if (hitEntity->AttackEntityFrom(player.get(), 0, DamageType::None)) {
			hookedEntity = entityManager->GetEntityByIdShared(hitEntity->id);
			velocity = {};
			position = end;
			RebuildCollider();
			return;
		}
		blockHit.hit = false;
	}

	if (blockHit.hit) {
		inGround = true;
		tilePosition = blockHit.blockPosition;
		inTile = blockHit.hitBlock.type;
		tileMeta = blockHit.hitBlock.data;
		return;
	}

	position = end;
	RebuildCollider();
	const bool submerged = world->IsAabbInFluidLevel(collider, Material::Water());
	float drag = 0.92f;

	if (submerged) {
		if (catchableTicks > 0) {
			velocity.y -= double(rand.NextFloat() * rand.NextFloat() * rand.NextFloat()) * 0.2;
		} else {
			const Int3 above{ MathHelper::FloorDouble(position.x), MathHelper::FloorDouble(position.y) + 1,
				              MathHelper::FloorDouble(position.z) };
			int wait = world->CanRainHitSpot(above) ? 300 : 500;
			if (world->rand.NextInt(wait) == 0) {
				catchableTicks = world->rand.NextInt(30) + 10;
				velocity.y -= 0.2;
			}
		}

		// scales with how much of the bobber is submerged
		double fraction = 0.0;
		constexpr int SLICES = 5;
		for (int i = 0; i < SLICES; ++i) {
			double minY = collider.minY + (collider.maxY - collider.minY) * double(i) / SLICES;
			double maxY = collider.minY + (collider.maxY - collider.minY) * double(i + 1) / SLICES;
			if (world->IsAabbInFluidLevel({ collider.minX, minY, collider.minZ, collider.maxX, maxY, collider.maxZ },
			                              Material::Water()))
				fraction += 1.0 / SLICES;
		}

		double waterFraction = fraction;
		velocity.y += 0.04 * (waterFraction * 2.0 - 1.0);
		if (waterFraction > 0.0) {
			drag *= 0.9f;
			velocity.y *= 0.8;
		}
	} else {
		velocity.y -= 0.03;
	}

	velocity.x *= drag;
	velocity.y *= drag;
	velocity.z *= drag;
}
