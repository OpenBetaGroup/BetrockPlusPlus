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
#include "entity_manager.h"
#include "java_random.h"
#include "nbt/nbt_tag.h"
#include "packet_data.h"
#include <optional>
#include <vector>

struct SlimeEntity : public HostileEntity {
	int size = 1;
	int slimeJumpDelay = 0;

	SlimeEntity() : HostileEntity() {
		type = EntityType::SLIME;
		burnInDaylight = false;
		slimeJumpDelay = rand.NextInt(20) + 10;
		SetSlimeSize(1 << rand.NextInt(3));
	}
	~SlimeEntity() = default;
	void Tick() override;
	void OnDeath(Entity* _killer, DamageType _type) override;
	void UpdateAIState() override;
	void EncodeMetadata(std::vector<PacketData::EntityMetadata::DataEntry>& _metadata) override;
	bool DecodeMetadata(const std::vector<PacketData::EntityMetadata::DataEntry>& _metadata) override;
	std::optional<Tag> SerializeToNbt() override;
	void LoadFromNbt(Tag& _nbt) override;
	void OnCollideWithPlayer(PlayerEntity& _entity) override;
	bool CanSpawnAt() override;
	void SplitSlime();

	// Valid sizes are:
	// 1, 2, 4, and 8
	void SetSlimeSize(int _size) {
		this->wasMetadataUpdated = true;
		this->size = _size;
		this->SetSize({ 0.6f * _size, 0.6f * _size });
		this->SetMaxHealth(_size * _size);
		this->Teleport(this->position, { this->rotationYaw, this->rotationPitch });
	}
};