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
#include "helpers/aabb.h"
#include "inventory/item_stack.h"
#include "java_random.h"
#include "nbt/nbt_tag.h"
#include "numeric_structs.h"
#include <cstdint>
#include <optional>

struct ItemEntity : public Entity {
	ItemStack itemStack;
	int8_t health = 5;
	int8_t pickupCooldown = 10;
	ItemEntity(Vec3 _position) : Entity() {
		type = EntityType::ITEM;
		width = 0.25f;
		height = 0.25f;
		yOffset = 0.125f; // height / 2
		stepHeight = 0.0f;

		// Set the initial position of the item entity
		this->Teleport(_position);

		// This stuff is mostly randomized
		rotationYaw = float(rand.NextDouble() * 360.0);
		velocity.x = float(rand.NextDouble() * 0.2 - 0.1);
		velocity.y = 0.2;
		velocity.z = float(rand.NextDouble() * 0.2 - 0.1);
	}
	void OnCollideWithPlayer(PlayerEntity& _entity) override;
	void Tick() override;
	std::optional<Tag> SerializeToNbt() override;
	void LoadFromNbt(Tag& _nbt) override;
	void UpdateFallState(float _movedY) override;
	AABB GetFluidCollider() override {
		// Returns the collider we use to compare if we are in a fluid
		return collider;
	}
	AABB GetLavaCollider() override {
		// Returns the collider we use to detect if we are in lava
		return collider;
	}
	bool AttackEntityFrom(Entity* _entity, int _damage, DamageType _type) override {
		Entity::AttackEntityFrom(_entity, _damage, _type);
		health -= _damage;
		if (health <= 0)
			isDead = true;
		return false;
	}
};