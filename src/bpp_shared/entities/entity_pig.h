/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#pragma once
#include "entity_animal.h"
#include "entity_zombie_pig.h"

struct PigEntity : public AnimalEntity {
	bool isSaddled = false;
	PigEntity() : AnimalEntity() {
		type = EntityType::PIG;
		width = 0.9f;
		height = 0.9f;
	}
	~PigEntity() = default;
	bool TryDespawn() override;
	void OnDeath(Entity* _killer, DamageType _type) override;
	void OnPlayerInteract(PlayerEntity* _entity) override;
	void EncodeMetadata(std::vector<PacketData::EntityMetadata::DataEntry>& _metadata) override;
	bool DecodeMetadata(const std::vector<PacketData::EntityMetadata::DataEntry>& _metadata) override;
	std::optional<Tag> SerializeToNbt() override;
	void LoadFromNbt(Tag& _nbt) override;

	void OnStruckByLightning(LightningEntity& /*_bolt*/) override {
		if (!world)
			return;

		std::shared_ptr<ZombiePigEntity> zombiePig = std::make_shared<ZombiePigEntity>();
		zombiePig->world = world;
		zombiePig->entityManager = entityManager;
		zombiePig->Teleport(this->position, { this->rotationYaw, this->rotationPitch });
		entityManager->AddEntity(zombiePig);

		this->isDead = true;
	}
};