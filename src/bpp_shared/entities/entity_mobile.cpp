/*
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */

#include "entity_mobile.h"
#include "addon/addon_impl.h"
#include "entities/entity.h"
#include "entities/entity_player.h"
#include "inventory/item_stack.h"
#include "java_math.h"
#include "world.h"
#include <algorithm>
#include <cmath>
#include <optional>

MobileEntity::MobileEntity() {
	stepHeight = 0.5;
	preventEntitySpawning = true;
}

// TODO: Pass those into API as needed :p
void MobileEntity::OnDeath(Entity* /*_killer*/, DamageType /*_type*/) {
	health = 0;
}

void MobileEntity::LoadFromNbt(Tag& _nbt) {
	Entity::LoadFromNbt(_nbt);

	this->health = _nbt.Has("Health") ? _nbt.compound["Health"].GetShort() : maxHealth;
	this->hurtResistantTime = _nbt.Has("HurtTime") ? _nbt.compound["HurtTime"].GetShort() : 0;
	this->deathTime = _nbt.Has("DeathTime") ? _nbt.compound["DeathTime"].GetShort() : 0;
	this->attackTime = _nbt.Has("AttackTime") ? _nbt.compound["AttackTime"].GetShort() : 0;
}

std::optional<Tag> MobileEntity::SerializeToNbt() {
	auto tag = Entity::SerializeToNbt();
	if (!tag)
		return std::nullopt;

	// Our additions
	Tag healthTag;
	healthTag.name = "Health";
	healthTag.type = TAG_SHORT;
	healthTag.shortValue = this->health;
	Tag hurtTimeTag;
	hurtTimeTag.name = "HurtTime";
	hurtTimeTag.type = TAG_SHORT;
	hurtTimeTag.shortValue = this->hurtResistantTime;
	Tag deathTimeTag;
	deathTimeTag.name = "DeathTime";
	deathTimeTag.type = TAG_SHORT;
	deathTimeTag.shortValue = this->deathTime;
	Tag attackTimeTag;
	attackTimeTag.name = "AttackTime";
	attackTimeTag.type = TAG_SHORT;
	attackTimeTag.shortValue = this->attackTime;

	// Add our additions to the base tag
	tag->compound["Health"] = healthTag;
	tag->compound["HurtTime"] = hurtTimeTag;
	tag->compound["DeathTime"] = deathTimeTag;
	tag->compound["AttackTime"] = attackTimeTag;

	return tag;
}

bool MobileEntity::onLadder() {
	auto fd = MathHelper::FloorDouble;
	return world->GetBlockId({ fd(position.x), fd(collider.minY), fd(position.z) }) == BLOCK_LADDER;
}

void MobileEntity::SetGoal(std::optional<Int3> _goal) {
	if (!_goal) {
		currentPath.clear();
		currentPathIdx = 0;
		return;
	}
	auto fd = MathHelper::FloorDouble;
	Int3 start = { fd(collider.minX), fd(collider.minY), fd(collider.minZ) };
	currentPath = pathFinder.FindPath(start, *_goal, width, height);
	currentPathIdx = 0;
}

bool MobileEntity::FollowPath() {
	jumping = false;

	// Mobs bob/hop while in water or lava
	bool isCurrentlyInLava = world->IsMaterialInAabb(GetLavaCollider(), Material::Lava());
	if ((inWater || isCurrentlyInLava) && rand.NextFloat() < 0.8f) {
		jumping = true;
	}

	if (currentPath.empty()) {
		return false;
	}

	// 1% chance to abandon our path
	if (rand.NextInt(100) == 0) {
		currentPath.clear();
		currentPathIdx = 0;
		return false;
	}

	int footprintSpan = int(width + 1.0f);
	double halfSpan = footprintSpan * 0.5;

	bool justFinished = false;

	while (currentPathIdx < currentPath.size()) {
		Int3 p = currentPath[currentPathIdx];
		double dx = p.x + halfSpan - position.x;
		double dz = p.z + halfSpan - position.z;

		double threshold = width * 2.0;
		if (dx * dx + dz * dz > threshold * threshold)
			break;

		currentPathIdx++;
		if (currentPathIdx >= currentPath.size())
			justFinished = true;
	}

	if (currentPathIdx >= currentPath.size()) {
		currentPath.clear();
		currentPathIdx = 0;

		if (justFinished && collidedHorizontally)
			jumping = true;
	} else {
		Int3 target = currentPath[currentPathIdx];
		double dx = target.x + halfSpan - position.x;
		double dz = target.z + halfSpan - position.z;

		float targetYaw = std::atan2(dz, dx) * 180.0 / JavaMath::PI - 90.0;
		float yawDelta = targetYaw - rotationYaw;

		while (yawDelta < -180.0F)
			yawDelta += 360.0F;
		while (yawDelta >= 180.0F)
			yawDelta -= 360.0F;
		yawDelta = std::clamp(yawDelta, -30.0F, 30.0F);

		rotationYaw += yawDelta;

		input.y = movementSpeed;

		int floorY = MathHelper::FloorDouble(collider.minY + 0.5);
		double dy = target.y - floorY;

		if (dy > 0.0)
			jumping = true;
	}

	return true;
}

void MobileEntity::ResolveEntityCollision(Entity& _other) {
	if (vehicle.lock().get() == &_other || passenger.lock().get() == &_other)
		return;

	Vec2 delta = { _other.position.x - position.x, _other.position.z - position.z };
	double dist = MathHelper::AbsMax(delta.x, delta.y);

	if (dist >= 0.01) {
		dist = std::sqrt(dist);

		delta = delta / dist;

		double forceScale = 1.0 / dist;

		if (forceScale > 1.0) {
			forceScale = 1.0;
		}

		delta = delta * forceScale;
		delta = delta * 0.05;

		this->velocity.x -= delta.x;
		this->velocity.z -= delta.y;
		_other.velocity.x += delta.x;
		_other.velocity.z += delta.y;
	}
}

void MobileEntity::TickPhysics() {
	bool currentlyInLava = world->IsMaterialInAabb(GetLavaCollider(), Material::Lava());
	if (inWater || currentlyInLava) {
		// Water and lava movement
		auto oldY = position.y;
		double friction = inWater ? double(0.8) : double(0.5);
		ApplyInput(0.02f);
		Move(this->velocity);
		velocity *= friction;
		velocity.y -= 0.02; // Sink

		AABB offsetCollider = collider.Offset(velocity.x, velocity.y + 0.6 - position.y + oldY, velocity.z);

		// Check if we are colliding with a block and we are
		// Moving up and unobstructed, if so, apply a nudge
		if (collidedHorizontally && AABBNotInLiquidOrObstructed(offsetCollider)) {
			velocity.y = 0.3;
		}
	} else {
		// Normal ground/air movement
		float friction = 0.91f;

		if (onGround) {
			friction = 0.546f;
			auto newBelowBlock = world->GetBlockId({ MathHelper::FloorDouble(position.x),
			                                         MathHelper::FloorDouble(position.y) - 1,
			                                         MathHelper::FloorDouble(position.z) });
			if (newBelowBlock > BLOCK_AIR) {
				friction = Blocks::blockProperties[newBelowBlock < BLOCK_MAX ? newBelowBlock : BLOCK_MAX].slipperiness *
				           0.91f;
			}
		}

		float acceleration = 0.16277136f / (friction * friction * friction);
		ApplyInput(onGround ? 0.1f * acceleration : 0.02f);

		// Clamp our velocity if we are touching a ladder
		// Also make sure we stop if we sneak
		if (onLadder()) {
			double maxLadderVelocity = 0.15;
			velocity.x = std::min(maxLadderVelocity, std::max(-maxLadderVelocity, velocity.x));
			velocity.z = std::min(maxLadderVelocity, std::max(-maxLadderVelocity, velocity.z));
			velocity.y = std::max(-maxLadderVelocity, velocity.y);

			// No fall damage on ladders
			fallDistance = 0.0f;

			// If sneaking we dont descend
			if (flags.isSneaking && velocity.y < 0.0)
				velocity.y = 0.0;
		}

		// Move the entity
		Move(this->velocity);

		// Our entity is pushing itself into the wall the ladder is on
		// So apply an upwards nudge
		if (collidedHorizontally && onLadder()) {
			velocity.y = 0.2;
		}

		// Gravity, then drag (Java: motionY -= 0.08; motionY *= 0.98)
		velocity.y -= 0.08;
		velocity.y *= 0.98f;

		// Apply friction
		velocity.x *= friction;
		velocity.z *= friction;
	}
}

void MobileEntity::ResolveEntityPushes() {
	auto collidingEntities = world->entityManager.GetEntitiesWithinAabbExcluding(collider.Expand(0.2, 0.0, 0.2), id);

	for (const auto& entity : collidingEntities) {
		if (!entity->CanBePushed())
			continue;

		// This is what lets minecarts pick up mobs and give them a quarter push
		if (auto* mobile = dynamic_cast<MobileEntity*>(entity))
			mobile->ResolveEntityCollision(*this);
		else
			this->ResolveEntityCollision(*entity); // Generic push is symmetric
	}
}

bool MobileEntity::AABBNotInLiquidOrObstructed(AABB& _collider) {
	auto collidedWith = world->GetCollidingBoundingBoxes(_collider, this);
	if (collidedWith.size() > 0)
		return false;
	return !world->IsLiquidInAabb(_collider);
}

void MobileEntity::Move(Vec3& _velocity) {
	Vec3 startPos = this->position;
	Entity::Move(_velocity);

	auto delta = position - startPos;

	if (canTriggerWalking && vehicle.expired()) {
		distanceWalkedModified += std::sqrt(delta.x * delta.x + delta.z * delta.z) * 0.6;

		int bx = MathHelper::FloorDouble(position.x);
		int by = MathHelper::FloorDouble(position.y - 0.2 - yOffset);
		int bz = MathHelper::FloorDouble(position.z);
		BlockType block = world->GetBlockId({ bx, by, bz });
		if (world->GetBlockId({ bx, by - 1, bz }) == BLOCK_FENCE)
			block = world->GetBlockId({ bx, by - 1, bz });

		if (distanceWalkedModified > float(nextStepDistance) && block != BLOCK_AIR) {
			++nextStepDistance;
			if (auto func = Blocks::blockBehaviors[block].onEntityWalking)
				func(*this->world, { bx, by, bz }, *this);
		}
	}
}

bool MobileEntity::HeadInOpaqueBlock() {
	// Check 8 corners of a slightly shrunk hitbox
	for (int corner = 0; corner < 8; corner++) {
		Float3 offset;
		offset.x = (float((corner >> 0) % 2) - 0.5f) * width * 0.9f;
		offset.y = (float((corner >> 1) % 2) - 0.5f) * 0.1f;
		offset.z = (float((corner >> 2) % 2) - 0.5f) * width * 0.9f;
		auto fd = MathHelper::FloorDouble;
		Int3 cornerBlockPos = { fd(position.x + offset.x), fd(position.y + GetEyeHeight() + offset.y),
			                    fd(position.z + offset.z) };
		if (world->IsBlockNormalCube(cornerBlockPos))
			return true;
	}
	return false;
}

bool MobileEntity::HeadInWater() {
	auto eyePosY = position.y + GetEyeHeight();
	auto fd = MathHelper::FloorDouble;
	Int3 headBlockPos = { fd(position.x), fd(eyePosY), fd(position.z) };
	auto headBlock = world->GetBlockId(headBlockPos);

	if (headBlock == BLOCK_WATER_STILL || headBlock == BLOCK_WATER_FLOWING) {
		float percentAir = Blocks::GetFluidPercentAir(world->GetMetadata(headBlockPos));
		float surfaceHeight = float(headBlockPos.y + 1) - (percentAir - 0.1111111f);
		return eyePosY < double(surfaceHeight);
	}
	return false;
}

void MobileEntity::Heal(int _health) {
	health = std::min(health + _health, maxHealth);
}

void MobileEntity::DealDamage(int _damage) {
	// Mark we have been attacked
	beenAttacked = true;

	// Damage our armor with the raw damage
	for (int i = 0; i < 4; i++) {
		auto thisPiece = armor[i];
		if (thisPiece && Items::IsArmor(thisPiece->id))
			Items::DamageItem(*thisPiece, _damage);
	}

	// Calculate damage with armor
	// Idk why armor uses a 25 point scale but it is what it is
	// It only ever goes up to 20, so a full set of armor blocks 80% of damage
	// Anything leftover damage wise gets accumulated until its enough to do a hit point
	int unmitigated = 25 - GetArmorValue();
	int scaledWithCarry = (_damage * unmitigated) + this->accumulatedFractionalDamage;
	int reducedDamage = scaledWithCarry / 25;
	accumulatedFractionalDamage = scaledWithCarry % 25;
	health -= reducedDamage;
}

int MobileEntity::GetArmorValue() {
	int totalProtection = 0;
	int totalRemainingDurability = 0;
	int totalMaxDurability = 0;

	for (int i = 0; i < 4; i++) {
		auto thisPiece = armor[i];
		if (thisPiece && Items::IsArmor(thisPiece->id)) {
			int maxDurability = Items::GetMaxDurability(thisPiece->id);
			int remainingDurability = maxDurability - thisPiece->data;

			totalRemainingDurability += remainingDurability;
			totalMaxDurability += maxDurability;
			totalProtection += Items::GetArmorDamageReduction(thisPiece->id);
		}
	}

	return totalMaxDurability == 0 ? 0 : (totalProtection - 1) * totalRemainingDurability / totalMaxDurability + 1;
}

bool MobileEntity::AttackEntityFrom(Entity* _entity, int _damage, DamageType _type) {
	age = 0;
	if (health <= 0)
		return false;

	bool freshHit = true;

	if (hurtResistantTime > maxHurtTime / 2.0f) {
		if (_damage <= lastAttackDamage)
			return false;

		DealDamage(_damage - lastAttackDamage);
		lastAttackDamage = _damage;
		freshHit = false;
	} else {
		lastAttackDamage = _damage;
		hurtResistantTime = maxHurtTime;
		DealDamage(_damage);
	}

	if (freshHit) {
		QueueEvent(PacketData::EntityEvent::HURT);
		if (_entity != nullptr) {
			auto dx = _entity->position.x - position.x;
			auto dz = _entity->position.z - position.z;

			// We are super close so just randomize
			while (dx * dx + dz * dz < 0.0001) {
				dx = (double(rand.NextInt()) / INT_MAX - double(rand.NextInt()) / INT_MAX) * 0.01;
				dz = (double(rand.NextInt()) / INT_MAX - double(rand.NextInt()) / INT_MAX) * 0.01;
			}

			attackedAtYaw = float(std::atan2(dz, dx) * 180.0 / JavaMath::PI) - rotationYaw;
			double dist = std::sqrt(dx * dx + dz * dz);
			auto activePassenger = this->passenger.lock();
			if (!(activePassenger && _entity == activePassenger.get()))
				ApplyKnockback({ float(dx / dist), 0.0f, float(dz / dist) });
		} else {
			attackedAtYaw = float(int(double(rand.NextInt() / INT_MAX * 2.0) * 180));
		}
	}

	if (health <= 0) {
		QueueEvent(PacketData::EntityEvent::DEATH);
		OnDeath(_entity, _type);
	}

	return true;
}

void MobileEntity::Tick() {
	age++;

	if (pathFinder.world == nullptr) {
		pathFinder.world = world;
	}

	Entity::Tick();

	// Suffocation
	if (HeadInOpaqueBlock()) {
		AttackEntityFrom(nullptr, 1, DamageType::Suffocation);
	}

	// Drowning
	if (HeadInWater() && !canBreatheUnderwater) {
		air--;
		if (air <= -20) {
			AttackEntityFrom(nullptr, 2, DamageType::Drown);
			air = 0;
		}
	} else {
		air = maxAir;
	}

	// Reset timers
	attackTime = std::max(0, attackTime - 1);
	hurtResistantTime = std::max(0, hurtResistantTime - 1);

	// Timer for the death animation
	if (health <= 0) {
		this->deathTime++;
		isDead = deathTime >= 20;
	}

	// Blocked entities drop their inputs
	if (IsMovementBlocked()) {
		jumping = false;
		input = {};
	} else {
		UpdateAIState();
	}

	// Jump code
	// Vanilla does this check here again with the current collider
	bool isCurrentlyInLava = world->IsMaterialInAabb(GetLavaCollider(), Material::Lava());
	if (jumping) {
		if (inWater || isCurrentlyInLava) {
			velocity.y += double(0.04);
		} else if (onGround) {
			velocity.y = double(0.42);
		}
	}

	// Movement easing
	input.x *= 0.98f;
	input.y *= 0.98f;

	if (hasPhysics)
		TickPhysics();

	ResolveEntityPushes();
}

ItemStack* MobileEntity::GetHeldItem() {
	return &heldItem;
}

void MobileEntity::SetHeldItem(ItemStack _stack) {
	heldItem = _stack;
}