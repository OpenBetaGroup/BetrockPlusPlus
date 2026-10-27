/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#include "entity_player.h"
#include "direction_fixer.h"
#include "entity_item.h"
#include "gamerules.h"
#include "logger/logger.h"

bool PlayerEntity::AttackEntityFrom(Entity* _entity, int _damage, DamageType _type) {
	auto success = MobileEntity::AttackEntityFrom(_entity, _damage, _type);
	if (success && isSleeping)
		WakeUp();
	if (success && dynamic_cast<MobileEntity*>(_entity)) {
		if (!_entity->isDead) 
			this->entityManager->AlertWolves(*this, *_entity);
	}
	return success;
}

void PlayerEntity::OnEnterPortal() {
	if (this->portalCooldown > 0) {
		this->portalCooldown = 10;
		return;
	} 
	this->isInPortal = true;
}

SleepFailureReason PlayerEntity::TrySleep(Int3 _pos) {
	if (this->isSleeping)
		return SleepFailureReason::ALREADY_SLEEPING;

	if (this->world && this->world->IsDay())
		return SleepFailureReason::CANT_SLEEP_AT_TIME;

	auto dx = std::abs(position.x - double(_pos.x));
	auto dy = std::abs(position.y - double(_pos.y));
	auto dz = std::abs(position.z - double(_pos.z));

	if (dx > 3.0 || dy > 2.0 || dz > 3.0)
		return SleepFailureReason::TOO_FAR;

	if (this->world->GetBlockId(_pos) != BLOCK_BED)
		return SleepFailureReason::OTHER;

	// Successfully sleep!
	this->SetSize({ 0.2f, 0.2f });
	this->bedPosition = _pos;
	this->yOffset = 0.0f;

	auto dir = GetDirectionFromMeta(BLOCK_BED, world->GetMetadata(_pos));

	float offsetX = 0.5f;
	float offsetZ = 0.5f;

	switch (dir) {
	case Cardinal::Value::South:
		offsetZ = 0.9f;
		break;
	case Cardinal::Value::West:
		offsetX = 0.1f;
		break;
	case Cardinal::Value::North:
		offsetZ = 0.1f;
		break;
	case Cardinal::Value::East:
		offsetX = 0.9f;
		break;
	default:
		break;
	}

	this->Teleport({ double(_pos.x) + offsetX, double(_pos.y) + 0.7375, double(_pos.z) + offsetZ },
	               { rotationYaw, rotationPitch });

	this->ticksInBed = 0;
	this->isSleeping = true;
	this->spawnPos = { double(_pos.x), double(_pos.y), double(_pos.z) };

	this->velocity = {};

	return SleepFailureReason::SUCCESS;
}

void PlayerEntity::WakeUp(bool /*_confirmSpawn*/) {
	this->SetSize({ 0.6f, 1.8f });
	this->yOffset = 0.0f;
	this->isSleeping = false;
	this->ticksInBed = 0;

	auto bedDir = GetDirectionFromMeta(BLOCK_BED, world->GetMetadata(this->bedPosition));
	Int3 footPos = this->bedPosition.WithOffset(Cardinal::Opposite(bedDir));
	auto sleepPositions = Blocks::GetBedApproachSpots(*this->world, this->bedPosition, footPos);
	if (!sleepPositions.empty()) {
		Int3 wakeupSpot = sleepPositions[0];
		constexpr double GROUND_NUDGE = 0.06;
		Vec3 teleportPos = { double(wakeupSpot.x) + 0.5, double(wakeupSpot.y) + double(this->yOffset) + GROUND_NUDGE,
			                 double(wakeupSpot.z) + 0.5 };
		this->Teleport(teleportPos, { rotationYaw, rotationPitch });
	} else {
		Vec3 teleportPos = { double(footPos.x) + 0.5, double(footPos.y) + double(this->yOffset) + 0.1,
			                 double(footPos.z) + 0.5 };
		this->Teleport(teleportPos, { rotationYaw, rotationPitch });
	}
}

void PlayerEntity::OnMountEntity() {
	// stub
	return;
}

void PlayerEntity::OnDismountEntity() {
	// stub
	return;
}

void PlayerEntity::UsePortal() {
	// stub
	return;
}

bool PlayerEntity::PickupItem(ItemStack& /*_stack*/, EntityId /*_entityId*/) {
	return true;
}

void PlayerEntity::DropInventory() {
	return;
}

void PlayerEntity::Tick() {
	// NOTE: This needs to be done before the MobileEntity parent stuff is ticked,
	// since drowning damage is dealt there.
	canBreatheUnderwater = !Gamerule::Get<bool>(Gamerule::Id::DrowningDamage);
	isImmuneToFire = !Gamerule::Get<bool>(Gamerule::Id::FireDamage);
	MobileEntity::Tick();

	// Portal counters
	if (this->isInPortal) {
		auto lockVehicle = vehicle.lock();
		if (lockVehicle)
			this->UnmountEntity();

		this->timeInPortal += 0.0125f;
		if (this->timeInPortal >= 1.0f) {
			this->timeInPortal = 1.0f;
			this->portalCooldown = 10;
			this->UsePortal();
		}

		this->isInPortal = false;
	} else {
		this->timeInPortal -= 0.05f;
		this->timeInPortal = std::max(0.0f, timeInPortal);
	}

	if (this->portalCooldown > 0)
		this->portalCooldown--;

	this->portalCooldown = std::max(0, this->portalCooldown);

	// Sleeping counters
	if (isSleeping) {
		ticksInBed++;
	} else {
		ticksInBed = 0;
	}
}

void PlayerEntity::OnDeath(Entity* _killer, DamageType _type) {
	MobileEntity::OnDeath(_killer, _type);
	// TODO: Pass death cause to C ABI

	// Shrink to the "squished corpse" hitbox
	SetSize({ 0.2f, 0.2f });

	velocity.y = 0.1;

	// Fling away from whatever hit us
	velocity.x = double(-std::cos((attackedAtYaw + rotationYaw) * JavaMath::PI / 180.0f) * 0.1f);
	velocity.z = double(-std::sin((attackedAtYaw + rotationYaw) * JavaMath::PI / 180.0f) * 0.1f);

	this->forceVelocityUpdate = true;

	if (!Gamerule::Get<bool>(Gamerule::Id::KeepInventory))
		this->DropInventory();
}

bool PlayerEntity::DropItem(ItemStack _stack) {
	if (_stack.id == Items::Id::INVALID || _stack.count <= 0)
		return false;

	// Create the item entity
	Vec3 itemPos = { position.x, position.y - 0.3 + PLAYER_EYE_HEIGHT, position.z };
	std::shared_ptr<ItemEntity> itemEntity = std::make_shared<ItemEntity>(itemPos);
	itemEntity->itemStack = _stack;
	itemEntity->pickupCooldown = 40; // So we don't pick it up instantly

	// Give ourselves some random velocity based on look direction
	float initVelocity = 0.3f;
	itemEntity->velocity.x = double(-std::sin(this->rotationYaw / 180.0F * JavaMath::PI_FLOAT) *
	                                std::cos(this->rotationPitch / 180.0F * JavaMath::PI_FLOAT) * initVelocity);
	itemEntity->velocity.z = double(std::cos(this->rotationYaw / 180.0F * JavaMath::PI_FLOAT) *
	                                std::cos(this->rotationPitch / 180.0F * JavaMath::PI_FLOAT) * initVelocity);
	itemEntity->velocity.y = double(-std::sin(this->rotationPitch / 180.0F * JavaMath::PI_FLOAT) * initVelocity + 0.1F);

	// Add a little bit of randomness
	initVelocity = 0.02f;
	float angle = rand.NextFloat() * JavaMath::PI_FLOAT * 2.0f;
	initVelocity *= rand.NextFloat();
	itemEntity->velocity.x += std::cos(angle) * initVelocity;
	itemEntity->velocity.y += (rand.NextFloat() - rand.NextFloat()) * 0.1f;
	itemEntity->velocity.z += std::sin(angle) * initVelocity;

	// Register our item with the world
	this->world->entityManager.AddEntity(std::move(itemEntity));
	return true;
}