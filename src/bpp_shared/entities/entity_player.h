/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#pragma once
#include "entity_mobile.h"

enum SleepFailureReason : uint8_t {
	CANT_SLEEP_AT_POSITION,
	CANT_SLEEP_AT_TIME,
	TOO_FAR,
	ALREADY_SLEEPING,
	OTHER,
	SUCCESS
};

struct FishingBobberEntity;
struct PlayerEntity : public MobileEntity {
	// Position of the bed (headboard) the player is currently sleeping in.
	// Only meaningful while isSleeping is true.
	Int3 bedPosition{ 0, 0, 0 };
	Vec3 spawnPos = {};
	std::weak_ptr<FishingBobberEntity> fishingBobber;
	
	// Also called "SleepTimer" in the player save
	TickTime ticksInBed = 0;
	float timeInPortal = 0.0f;
	int portalCooldown = 0;
	bool isSleeping : 1 = false;
	bool isInPortal : 1 = false;

	// Only used for wolves! Annoying!
	std::string username;
	PlayerEntity() : MobileEntity() {
		type = EntityType::PLAYER;
		width = 0.6f;
		height = 1.8f;
		stepHeight = 0.5f;
	}
	PlayerEntity(std::string _username) : MobileEntity() {
		type = EntityType::PLAYER;
		width = 0.6f;
		height = 1.8f;
		stepHeight = 0.5f;
		username = _username;
	}
	~PlayerEntity() = default;
	bool AttackEntityFrom(Entity* _entity, int _damage, DamageType _type) override;
	bool HeadInOpaqueBlock() override;
	virtual void OnEnterPortal() override;
	virtual void Tick() override;
	virtual bool PickupItem(ItemStack& _stack, EntityId _entityId);
	virtual bool DropItem(ItemStack _stack);
	virtual void DropInventory();
	virtual void OnDeath(Entity* _killer, DamageType _type) override;
	virtual void OnMountEntity() override;
	virtual void OnDismountEntity() override;
	virtual SleepFailureReason TrySleep(Int3 _pos);
	virtual void WakeUp(bool _confirmSpawn = true);
	virtual void UsePortal();
	
	float GetEyeHeight() override {
		return 1.62f;
	}
	
	float GetRidingYOffset() override {
		return yOffset - 0.5f;
	}

	bool IsMovementBlocked() override {
		return health <= 0 || isSleeping;
	}
};