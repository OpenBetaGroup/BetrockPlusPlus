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
#include "entity_animal.h"
#include "java_random.h"
#include "nbt/nbt_tag.h"
#include "packet_data.h"
#include <cstdint>
#include <optional>
#include <vector>

struct SheepEntity : public AnimalEntity {
	// TODO: Replace with an Enum
	// TODO: Could possibly combine with isSheared via 4-bit + 1-bit value?
	int8_t color = 0;
	bool isSheared = false;

	SheepEntity() : AnimalEntity() {
		type = EntityType::SHEEP;
		width = 0.9f;
		height = 1.3f;
		color = RollFleeceColor();
		SetMaxHealth(/*Health=*/8);
	}
	~SheepEntity() = default;
	void OnDeath(Entity* _killer, DamageType _type) override;
	void EncodeMetadata(std::vector<PacketData::EntityMetadata::DataEntry>& _metadata) override;
	bool DecodeMetadata(const std::vector<PacketData::EntityMetadata::DataEntry>& _metadata) override;
	std::optional<Tag> SerializeToNbt() override;
	void LoadFromNbt(Tag& _nbt) override;

	int RollFleeceColor() {
		int roll = rand.NextInt(100);
		if (roll < 5)
			return 15; // Black
		else if (roll < 10)
			return 7; // Gray
		else if (roll < 15)
			return 8; // Light Gray
		else if (roll < 18)
			return 12; // Brown
		else
			return rand.NextInt(500) == 0 ? 6 : 0; // Pink or White
	}
};
