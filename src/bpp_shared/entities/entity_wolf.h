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
	bool shakeActive : 1 = false;
	float shakeTime = 0.0f;
	int previousSentHealth = -1;
	WolfEntity() : AnimalEntity() {
		type = EntityType::WOLF;
		movementSpeed = 1.1f;
		SetMaxHealth(8);
		SetSize({ 0.8, 0.8 });
	}
	~WolfEntity() = default;
	bool IsTamed();
	void UpdateAIState() override;
	bool TryDespawn() override;
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
	void PathToOrTPToOwner(PlayerEntity& _owner, float _distance);
	
	int GetAttackStrength() {
		return IsTamed() ? 4 : 2;
	}

	bool IsMovementBlocked() override {
		return health <= 0 || isSitting || shakeActive;
	}

	void SetWolfAngry(bool _angry) {
		this->isAngry = _angry;
		this->wasMetadataUpdated = true;
	}

	void SetWolfTamed(std::string _user) {
		this->ownerUsername = _user;
		this->wasMetadataUpdated = true;
	}

	void SetWolfSitting(bool _sitting) {
		this->isSitting = _sitting;
		this->wasMetadataUpdated = true;
	}
};