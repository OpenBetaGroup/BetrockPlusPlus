/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, jomner <github.com/jomner>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once
#include "entities.h"
#include "entities/entity_player.h"
#include "inventory/item_stack.h"

struct PlayerSession;
struct EntityMPPlayer : public PlayerEntity {
	PlayerSession* session = nullptr;
	EntityHealth lastNotifiedHealth = health;
	bool movedThisTick : 1 = false;
	int ticksInAir = 0;
	int32_t ticksSinceTeleportSent = 0;
	double accumulatedUpDistance = 0.0;
	double simulatedFallDistance = 0.0;
	Vec3 firstUpPosition = {};
	Vec3 previousVelocity = {};
	
	EntityMPPlayer() : PlayerEntity() {
		hasPhysics = false;
	}

	EntityMPPlayer(std::string _username) : PlayerEntity(_username) {
		hasPhysics = false;
	}

	~EntityMPPlayer() {
		session = nullptr;
	}
	virtual void Tick() override;
	void Move(Vec3& _velocity) override;
	bool PickupItem(ItemStack& _stack, EntityId _entityId) override;
	void UpdateFallState(float _movedY) override;
	void OnDeath(Entity* _killer, DamageType _type) override;
	bool DropItem(ItemStack _stack) override;
	void HandlePositionChecks();
	void DropInventory() override;
	void OnMountEntity() override;
	void OnDismountEntity() override;
	void UsePortal() override;
	void OnEnterPortal() override;
	ItemStack* GetHeldItem() override;
	virtual SleepFailureReason TrySleep(Int3 _pos);
	virtual void WakeUp(bool _confirmSpawn = true) override;
};