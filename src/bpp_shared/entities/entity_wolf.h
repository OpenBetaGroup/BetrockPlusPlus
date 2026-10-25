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
	std::string ownerUsername;
	std::weak_ptr<Entity> attackTarget;
	bool isSitting : 1 = false;
	bool isAngry : 1 = false;
	bool hasAttacked : 1 = false;
	WolfEntity() : AnimalEntity() {
		type = EntityType::WOLF;
		movementSpeed = 1.1f;
		SetMaxHealth(8);
		SetSize({ 0.8, 0.8 });
	}
	~WolfEntity() = default;

	int GetAttackStrength() {
		return IsTamed() ? 4 : 2;
	}
	bool IsTamed();
	void UpdateAIState() override;
	bool AttackEntityFrom(Entity* _entity, int _damage, DamageType _type) override;
	void Tick() override;
	void OnPlayerInteract(PlayerEntity* _entity) override;
	void OnDeath(Entity* _killer, DamageType _type) override;
	void EncodeMetadata(std::vector<PacketData::EntityMetadata::DataEntry>& _metadata) override;
	bool DecodeMetadata(const std::vector<PacketData::EntityMetadata::DataEntry>& _metadata) override;
	std::optional<Tag> SerializeToNbt() override;
	void LoadFromNbt(Tag& _nbt) override;
	virtual std::shared_ptr<Entity> FindPlayerToAttack();
	virtual void TryAttackEntity(Entity& _target, float _distance);
	virtual void OnTargetLostSight(Entity& /*_target*/, float /*_distance*/) {}
};