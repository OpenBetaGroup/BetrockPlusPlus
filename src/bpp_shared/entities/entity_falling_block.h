/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#pragma once
#include "entity.h"

struct FallingBlockEntity : public Entity {
	BlockType block = BLOCK_AIR;
	int ticksFallen = 0;

	FallingBlockEntity(Vec3 _spawnPosition, BlockType _block) : Entity() {
		this->block = _block;
		this->position = _spawnPosition;
		this->height = 0.98;
		this->width = 0.98;
		this->yOffset = height / 2.0f;
		this->preventEntitySpawning = true;
		RebuildCollider();

		if (_block == BLOCK_SAND)
			type = EntityType::FALLING_SAND;
		if (_block == BLOCK_GRAVEL)
			type = EntityType::FALLING_GRAVEL;
	}

	void Tick() override;

	std::optional<Tag> SerializeToNbt() override {
		auto root = Entity::SerializeToNbt();
		if (!root || this->block == BLOCK_AIR)
			return std::nullopt;
		Tag tile;
		tile.type = TAG_BYTE;
		tile.name = "Tile";
		tile.byteValue = this->block;
		root->compound["Tile"] = tile;
		return root;
	}

	void LoadFromNbt(Tag& _nbt) override {
		Entity::LoadFromNbt(_nbt);
		this->block = BlockType(_nbt.compound["Tile"].GetByte() & 255);
	}

	bool CanBeCollidedWith() override {
		return !isDead;
	}
};