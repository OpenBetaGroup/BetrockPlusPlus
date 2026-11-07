/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#pragma once

#include "entity.h"
#include "logger.h"

struct ArtProfile {
	std::string title = "Kebab";
	int sizeX = 0;
	int sizeY = 0;
	int offsetX = 0;
	int offsetY = 0;
};

struct PaintingEntity : public Entity {
	ArtProfile art = {};
	Cardinal::Value direction = Cardinal::Value::None;
	TickTime tickCounter = 0;
	Int3 blockCoordinates = {};
	std::string validPaintingNames[25] = { "Kebab",   "Aztec",     "Alban",        "Aztec2",        "Bomb",
		                                   "Plant",   "Wasteland", "Pool",         "Courbet",       "Sea",
		                                   "Sunset",  "Creebet",   "Wanderer",     "Graham",        "Match",
		                                   "Bust",    "Stage",     "Void",         "SkullAndRoses", "Fighters",
		                                   "Pointer", "Pigscene",  "BurningSkull", "Skeleton",      "DonkeyKong" };

	PaintingEntity() : Entity() {
		type = EntityType::PAINTING;
		width = 0.5f;
		height = 0.5f;
		yOffset = 0.0f; // height / 2
		stepHeight = 0.0f;
		SetDirection(direction);
	}

	PaintingEntity(Int3 _blockPos, Cardinal::Value _direction) : Entity() {
		type = EntityType::PAINTING;
		width = 0.5f;
		height = 0.5f;
		yOffset = 0.0f; // height / 2
		stepHeight = 0.0f;
		blockCoordinates = _blockPos;
		SetDirection(_direction);
	}

	std::optional<Tag> SerializeToNbt() override;
	void LoadFromNbt(Tag& _nbt) override;
	void SetDirection(Cardinal::Value _direction);
	bool OnValidSurface();

	bool CanBeCollidedWith() override {
		return true;
	}

	void Tick() override {
		tickCounter++;
		if (tickCounter >= 100) {
			tickCounter = 0;
			if (!OnValidSurface())
				KillAndDropPainting();
		}
	}

	bool PickValidArt() {
		std::vector<ArtProfile> validPaintings;
		for (int i = 0; i < 25; i++) {
			auto candidate = GetArtProfile(validPaintingNames[i]);
			this->art = candidate;
			this->SetDirection(direction);
			if (OnValidSurface())
				validPaintings.push_back(candidate);
		}

		if (validPaintings.empty())
			return false;

		// TODO: Can these turn out negative?
		this->art = validPaintings[size_t(this->rand.NextInt(validPaintings.size()))];
		this->SetDirection(direction);
		return true;
	}

	void KillAndDropPainting() {
		this->isDead = true;
		DropItemAtEntity(Items::PAINTING, 1);
	}

	void Move(Vec3& _velocity) override {
		if (std::abs(_velocity.x) > 0 || std::abs(_velocity.y) > 0 || std::abs(_velocity.z) > 0)
			KillAndDropPainting();
	}

	bool AttackEntityFrom(Entity* /*_entity*/, int /*_damage*/, DamageType /*_type*/) override {
		KillAndDropPainting();
		return true;
	}

	float GetArtSize(int pixelSize) {
		return (pixelSize == 64 || pixelSize == 32) ? 0.5f : 0.0f;
	}

	ArtProfile GetArtProfile(std::string title) {
		if (title == "Kebab")
			return { "Kebab", 16, 16, 0, 0 };
		if (title == "Aztec")
			return { "Aztec", 16, 16, 16, 0 };
		if (title == "Alban")
			return { "Alban", 16, 16, 32, 0 };
		if (title == "Aztec2")
			return { "Aztec2", 16, 16, 48, 0 };
		if (title == "Bomb")
			return { "Bomb", 16, 16, 64, 0 };
		if (title == "Plant")
			return { "Plant", 16, 16, 80, 0 };
		if (title == "Wasteland")
			return { "Wasteland", 16, 16, 96, 0 };
		if (title == "Pool")
			return { "Pool", 32, 16, 0, 32 };
		if (title == "Courbet")
			return { "Courbet", 32, 16, 32, 32 };
		if (title == "Sea")
			return { "Sea", 32, 16, 64, 32 };
		if (title == "Sunset")
			return { "Sunset", 32, 16, 96, 32 };
		if (title == "Creebet")
			return { "Creebet", 32, 16, 128, 32 };
		if (title == "Wanderer")
			return { "Wanderer", 16, 32, 0, 64 };
		if (title == "Graham")
			return { "Graham", 16, 32, 16, 64 };
		if (title == "Match")
			return { "Match", 32, 32, 0, 128 };
		if (title == "Bust")
			return { "Bust", 32, 32, 32, 128 };
		if (title == "Void")
			return { "Void", 32, 32, 96, 128 };
		if (title == "SkullAndRoses")
			return { "SkullAndRoses", 32, 32, 128, 128 };
		if (title == "Fighters")
			return { "Fighters", 64, 32, 0, 96 };
		if (title == "Pointer")
			return { "Pointer", 64, 64, 0, 192 };
		if (title == "Pigscene")
			return { "Pigscene", 64, 64, 64, 192 };
		if (title == "BurningSkull")
			return { "BurningSkull", 64, 64, 128, 192 };
		if (title == "Skeleton")
			return { "Skeleton", 64, 48, 192, 64 };
		if (title == "DonkeyKong")
			return { "DonkeyKong", 64, 48, 192, 112 };
		if (title == "Stage")
			return { "Stage", 32, 32, 64, 128 };
		// Default fallback, in case shit fucks up bad
		Logger::Warning << "Invalid painting requested: " << title << "!\n";
		return { "Kebab", 16, 16, 0, 0 };
	}
};