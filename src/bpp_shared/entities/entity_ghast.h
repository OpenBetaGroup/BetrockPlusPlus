/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#pragma once
#include "entity_hostile.h"

struct GhastEntity : public HostileEntity {
	Vec3 waypoint = {};
	int courseChangeCooldown = 0;
	int aggroCooldown = 0;

	// Counts up while we can see our target
	// Fire at 20 ticks then resets to -40
	int attackCounter = 0;

	// So we can tell the client to swap to the angry texture
	int8_t attackingFlag = 0;

	GhastEntity() : HostileEntity() {
		type = EntityType::GHAST;
		SetSize({ 4.0f, 4.0f });
		SetMaxHealth(10);
		isImmuneToFire = true;
		burnInDaylight = false;
	}
	~GhastEntity() = default;

	void Tick() override;
	void UpdateAIState() override;
	void TickPhysics() override;
	void OnDeath(Entity* _killer, DamageType _type) override;
	bool CanSpawnAt() override;
	void EncodeMetadata(std::vector<PacketData::EntityMetadata::DataEntry>& _metadata) override;
	bool DecodeMetadata(const std::vector<PacketData::EntityMetadata::DataEntry>& _metadata) override;

	void UpdateFallState(float /*_movedY*/) override {
		fallDistance = 0.0f;
	}
	bool IsOnLadder() override {
		return false;
	}

private:
	bool IsCourseTraversable(double _distance);
	void ShootFireball(double _dx, double _dy, double _dz);
};
