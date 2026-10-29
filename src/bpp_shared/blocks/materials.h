/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once
#include <cstdint>

// Map colors
struct MapColor {
	uint32_t index : 8 = 0;
	uint32_t colorValue : 24 = 0; // packed RGB

	static constexpr MapColor Air() {
		return { 0, 0x000000 };
	}
	static constexpr MapColor Grass() {
		return { 1, 0x7FB238 };
	}
	static constexpr MapColor Sand() {
		return { 2, 0xF7E9A3 };
	}
	static constexpr MapColor Cloth() {
		return { 3, 0xA7A7A7 };
	}
	static constexpr MapColor TNT() {
		return { 4, 0xFF0000 };
	}
	static constexpr MapColor Ice() {
		return { 5, 0xA0A0FF };
	}
	static constexpr MapColor Iron() {
		return { 6, 0xA7A7A7 };
	}
	static constexpr MapColor Foliage() {
		return { 7, 0x007C00 };
	}
	static constexpr MapColor Snow() {
		return { 8, 0xFFFFFF };
	}
	static constexpr MapColor Clay() {
		return { 9, 0xA4A8B8 };
	}
	static constexpr MapColor Dirt() {
		return { 10, 0xB7906F };
	}
	static constexpr MapColor Stone() {
		return { 11, 0x707070 };
	}
	static constexpr MapColor Water() {
		return { 12, 0x4040FF };
	}
	static constexpr MapColor Wood() {
		return { 13, 0x685432 };
	}
};

// Uses at most 5 bits
enum class MaterialType : uint8_t {
	Air,
	Grass,
	Ground, // dirt, gravel, clay
	Wood,
	Rock,
	Iron,
	Water,
	Lava,
	Leaves,
	Plants, // non-solid, no grass cover (flowers, saplings, etc)
	Sponge,
	Cloth, // wool, web
	Fire,
	Sand,
	Circuits, // redstone wire, rails, etc
	Glass,
	TNT,
	Coral,
	Ice,
	SnowLayer, // snow layer
	SnowBlock, // snow block
	Cactus,
	Clay,
	Pumpkin,
	Portal,
	Cake,
	Web,
	Piston,
};

// NOTE: This way it fits nicely into 2 bits!
enum PushabilityFlag : int8_t {
	Invalid = 0,
	Normal = 1,
	NoPush = 2,
	Immovable = 3,
};

struct Material {
	MapColor mapColor = MapColor::Stone();
	MaterialType type : 5 = MaterialType::Rock;
	PushabilityFlag mobilityFlag : 2 = PushabilityFlag::Normal;
	bool isLiquid : 1 = false;
	bool isSolid : 1 = true;
	bool isOpaque : 1 = true;
	bool isGroundCover : 1 = false;
	bool canBlockGrass : 1 = true;
	bool isHarvestable : 1 = true;

	bool operator==(const Material& _other) const {
		return type == _other.type;
	}
	bool operator!=(const Material& _other) const {
		return type != _other.type;
	}

	static constexpr Material Air() {
		Material m{};
		m.type = MaterialType::Air;
		m.mapColor = MapColor::Air();
		m.isSolid = false;
		m.isOpaque = false;
		m.canBlockGrass = false;
		m.isGroundCover = true;
		return m;
	}

	static constexpr Material Grass() {
		Material m{};
		m.type = MaterialType::Grass;
		m.mapColor = MapColor::Grass();
		return m;
	}

	static constexpr Material Ground() {
		Material m{};
		m.type = MaterialType::Ground;
		m.mapColor = MapColor::Dirt();
		return m;
	}

	static constexpr Material Wood() {
		Material m{};
		m.type = MaterialType::Wood;
		m.mapColor = MapColor::Wood();
		return m;
	}

	static constexpr Material Rock() {
		Material m{};
		m.type = MaterialType::Rock;
		m.mapColor = MapColor::Stone();
		m.isHarvestable = false;
		return m;
	}

	static constexpr Material Iron() {
		Material m{};
		m.type = MaterialType::Iron;
		m.mapColor = MapColor::Iron();
		m.isHarvestable = false;
		return m;
	}

	static constexpr Material Water() {
		Material m{};
		m.type = MaterialType::Water;
		m.mapColor = MapColor::Water();
		m.isLiquid = true;
		m.isSolid = false;
		m.isGroundCover = true;
		m.mobilityFlag = PushabilityFlag::NoPush;
		return m;
	}

	static constexpr Material Lava() {
		Material m{};
		m.type = MaterialType::Lava;
		m.mapColor = MapColor::TNT();
		m.isLiquid = true;
		m.isSolid = false;
		m.isGroundCover = true;
		m.mobilityFlag = PushabilityFlag::NoPush;
		return m;
	}

	static constexpr Material Leaves() {
		Material m{};
		m.type = MaterialType::Leaves;
		m.mapColor = MapColor::Foliage();
		m.isOpaque = false;
		m.mobilityFlag = PushabilityFlag::NoPush;
		return m;
	}

	static constexpr Material Plants() {
		Material m{};
		m.type = MaterialType::Plants;
		m.mapColor = MapColor::Foliage();
		m.isSolid = false;
		m.isOpaque = false;
		m.canBlockGrass = false;
		m.mobilityFlag = PushabilityFlag::NoPush;
		return m;
	}

	static constexpr Material Sponge() {
		Material m{};
		m.type = MaterialType::Sponge;
		m.mapColor = MapColor::Cloth();
		return m;
	}

	static constexpr Material Cloth() {
		Material m{};
		m.type = MaterialType::Cloth;
		m.mapColor = MapColor::Cloth();
		return m;
	}

	static constexpr Material Fire() {
		Material m{};
		m.type = MaterialType::Fire;
		m.mapColor = MapColor::Air();
		m.isSolid = false;
		m.isOpaque = false;
		m.canBlockGrass = false;
		m.isGroundCover = true;
		m.mobilityFlag = PushabilityFlag::NoPush;
		return m;
	}

	static constexpr Material Sand() {
		Material m{};
		m.type = MaterialType::Sand;
		m.mapColor = MapColor::Sand();
		return m;
	}

	// MaterialLogic + setNoPushMobility
	static constexpr Material Circuits() {
		Material m{};
		m.type = MaterialType::Circuits;
		m.mapColor = MapColor::Air();
		m.isSolid = false;
		m.isOpaque = false;
		m.canBlockGrass = false;
		m.mobilityFlag = PushabilityFlag::NoPush;
		return m;
	}

	static constexpr Material Glass() {
		Material m{};
		m.type = MaterialType::Glass;
		m.mapColor = MapColor::Air();
		m.isOpaque = false;
		return m;
	}

	static constexpr Material TNT() {
		Material m{};
		m.type = MaterialType::TNT;
		m.mapColor = MapColor::TNT();
		m.isOpaque = false;
		return m;
	}

	static constexpr Material Coral() {
		Material m{};
		m.type = MaterialType::Coral;
		m.mapColor = MapColor::Foliage();
		m.mobilityFlag = PushabilityFlag::NoPush;
		return m;
	}

	static constexpr Material Ice() {
		Material m{};
		m.type = MaterialType::Ice;
		m.mapColor = MapColor::Ice();
		m.isOpaque = false;
		return m;
	}

	static constexpr Material SnowLayer() {
		Material m{};
		m.type = MaterialType::SnowLayer;
		m.mapColor = MapColor::Snow();
		m.isSolid = false;
		m.isOpaque = false;
		m.isGroundCover = true;
		m.isHarvestable = false;
		m.canBlockGrass = false;
		m.mobilityFlag = PushabilityFlag::NoPush;
		return m;
	}

	static constexpr Material SnowBlock() {
		Material m{};
		m.type = MaterialType::SnowBlock;
		m.mapColor = MapColor::Snow();
		m.isHarvestable = false;
		return m;
	}

	static constexpr Material Cactus() {
		Material m{};
		m.type = MaterialType::Cactus;
		m.mapColor = MapColor::Foliage();
		m.isOpaque = false;
		m.mobilityFlag = PushabilityFlag::NoPush;
		return m;
	}

	static constexpr Material Clay() {
		Material m{};
		m.type = MaterialType::Clay;
		m.mapColor = MapColor::Clay();
		return m;
	}

	static constexpr Material Pumpkin() {
		Material m{};
		m.type = MaterialType::Pumpkin;
		m.mapColor = MapColor::Foliage();
		m.mobilityFlag = PushabilityFlag::NoPush;
		return m;
	}

	static constexpr Material Portal() {
		Material m{};
		m.type = MaterialType::Portal;
		m.mapColor = MapColor::Air();
		m.isSolid = false;
		m.isOpaque = false;
		m.canBlockGrass = false;
		m.mobilityFlag = PushabilityFlag::Immovable;
		return m;
	}

	static constexpr Material Cake() {
		Material m{};
		m.type = MaterialType::Cake;
		m.mapColor = MapColor::Air();
		m.mobilityFlag = PushabilityFlag::NoPush;
		return m;
	}

	static constexpr Material Web() {
		Material m{};
		m.type = MaterialType::Web;
		m.mapColor = MapColor::Cloth();
		m.isHarvestable = false;
		m.mobilityFlag = PushabilityFlag::NoPush;
		return m;
	}

	static constexpr Material Piston() {
		Material m{};
		m.type = MaterialType::Piston;
		m.mapColor = MapColor::Stone();
		m.mobilityFlag = PushabilityFlag::Immovable;
		return m;
	}
};