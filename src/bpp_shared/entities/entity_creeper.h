/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#pragma once
#include "damage_types.h"
#include "entities.h"
#include "entity.h"
#include "entity_hostile.h"
#include "nbt/nbt_tag.h"
#include "packet_data.h"
#include <cstdint>
#include <optional>
#include <vector>

struct CreeperEntity : public HostileEntity {
	// -1 = fuse unlit/counting down, 1 = fuse lit and counting up
	int8_t creeperState = -1;
	// Charged creeper
	bool powered = false;

	int timeSinceIgnited = 0;
	int lastActiveTime = 0;

	CreeperEntity() : HostileEntity() {
		type = EntityType::CREEPER;
		width = 0.6f;
		height = 1.8f;
	}
	~CreeperEntity() = default;

	void OnDeath(Entity* _killer, DamageType _type) override;
	void Tick() override;
	void TryAttackEntity(Entity& _target, float _distance) override;
	void OnTargetLostSight(Entity& _target, float _distance) override;
	void EncodeMetadata(std::vector<PacketData::EntityMetadata::DataEntry>& _metadata) override;
	bool DecodeMetadata(const std::vector<PacketData::EntityMetadata::DataEntry>& _metadata) override;
	std::optional<Tag> SerializeToNbt() override;
	void LoadFromNbt(Tag& _nbt) override;

	void SetPowered(bool _powered) {
		UpdateMetadata(powered, _powered);
	}

	void OnStruckByLightning(LightningEntity& _bolt) override {
		Entity::OnStruckByLightning(_bolt);

		SetPowered(true);
	}
};