/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#pragma once
#include "entity_animal.h"

struct WolfEntity : public AnimalEntity {
	std::string ownerUsername = "PixelBrushArt";
	bool isSitting : 1 = false;
	bool isAngry : 1 = false;
	WolfEntity() : AnimalEntity() {
		type = EntityType::WOLF;
		width = 0.9f;
		height = 1.3f;
	}
	~WolfEntity() = default;
	bool IsTamed();
	virtual void Tick() override;
	void OnDeath(Entity* _killer, DamageType _type) override;
	void EncodeMetadata(std::vector<PacketData::EntityMetadata::DataEntry>& _metadata) override;
	bool DecodeMetadata(const std::vector<PacketData::EntityMetadata::DataEntry>& _metadata) override;
	std::optional<Tag> SerializeToNbt() override;
	void LoadFromNbt(Tag& _nbt) override;
};