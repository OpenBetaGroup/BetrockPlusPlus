/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "aabb.h"
#include "helpers/direction_fixer.h"
#include <cstdint>

namespace Blocks {

// defaults
inline AABB DefaultAabb(uint8_t) {
	return { 0.0, 0.0, 0.0, 1.0, 1.0, 1.0 };
}
inline CollisionShape DefaultCollider(uint8_t) {
	CollisionShape s;
	s.Add({ 0.0, 0.0, 0.0, 1.0, 1.0, 1.0 });
	return s;
}

// slab
inline AABB SlabAabb(uint8_t) {
	return { 0.0, 0.0, 0.0, 1.0, 0.5, 1.0 };
}
inline CollisionShape SlabCollider(uint8_t) {
	CollisionShape s;
	s.Add({ 0.0, 0.0, 0.0, 1.0, 0.5, 1.0 });
	return s;
}

// stairs
inline CollisionShape StairCollider(uint8_t _meta) {
	CollisionShape s;
	switch (_meta & 3) {
	case 0:
		s.Add({ 0.0, 0.0, 0.0, 0.5, 0.5, 1.0 });
		s.Add({ 0.5, 0.0, 0.0, 1.0, 1.0, 1.0 });
		break;
	case 1:
		s.Add({ 0.0, 0.0, 0.0, 0.5, 1.0, 1.0 });
		s.Add({ 0.5, 0.0, 0.0, 1.0, 0.5, 1.0 });
		break;
	case 2:
		s.Add({ 0.0, 0.0, 0.0, 1.0, 0.5, 0.5 });
		s.Add({ 0.0, 0.0, 0.5, 1.0, 1.0, 1.0 });
		break;
	case 3:
		s.Add({ 0.0, 0.0, 0.0, 1.0, 1.0, 0.5 });
		s.Add({ 0.0, 0.0, 0.5, 1.0, 0.5, 1.0 });
		break;
	}
	return s;
}

// cactus
inline AABB CactusAabb(uint8_t) {
	constexpr double I = 0.0625;
	return { I, 0.0, I, 1.0 - I, 1.0, 1.0 - I };
}
inline CollisionShape CactusCollider(uint8_t) {
	constexpr double I = 0.0625;
	CollisionShape s;
	s.Add({ I, 0.0, I, 1.0 - I, 1.0 - I, 1.0 - I });
	return s;
}

// snow layer
inline AABB SnowLayerAabb(uint8_t _meta) {
	float h = (2.0f * (1 + (_meta & 7))) / 16.0f;
	return { 0.0, 0.0, 0.0, 1.0, h, 1.0 };
}
inline CollisionShape SnowLayerCollider(uint8_t _meta) {
	CollisionShape s;
	if ((_meta & 7) >= 3)
		s.Add({ 0.0, 0.0, 0.0, 1.0, 0.5, 1.0 });
	return s;
}

// ladder
inline AABB LadderAabb(uint8_t _meta) {
	constexpr double T = 0.125;
	switch (_meta) {
	case 2:
		return { 0.0, 0.0, 1.0 - T, 1.0, 1.0, 1.0 };
	case 3:
		return { 0.0, 0.0, 0.0, 1.0, 1.0, T };
	case 4:
		return { 1.0 - T, 0.0, 0.0, 1.0, 1.0, 1.0 };
	case 5:
		return { 0.0, 0.0, 0.0, T, 1.0, 1.0 };
	default:
		return { 0.0, 0.0, 0.0, 1.0, 1.0, 1.0 };
	}
}
inline CollisionShape LadderCollider(uint8_t _meta) {
	constexpr double T = 0.125;
	CollisionShape s;
	switch (_meta) {
	case 2:
		s.Add({ 0.0, 0.0, 1.0 - T, 1.0, 1.0, 1.0 });
		break;
	case 3:
		s.Add({ 0.0, 0.0, 0.0, 1.0, 1.0, T });
		break;
	case 4:
		s.Add({ 1.0 - T, 0.0, 0.0, 1.0, 1.0, 1.0 });
		break;
	case 5:
		s.Add({ 0.0, 0.0, 0.0, T, 1.0, 1.0 });
		break;
	}
	return s;
}

// door
// bits 0-1 = facing when closed, bit 2 = open, bit 3 = top half
inline int DoorState(uint8_t _meta) {
	return ((_meta & 4) == 0) ? ((_meta - 1) & 3) : (_meta & 3);
}
inline AABB DoorAabb(uint8_t _meta) {
	constexpr double T = 0.1875;
	switch (DoorState(_meta)) {
	case 0:
		return { 0.0, 0.0, 0.0, 1.0, 1.0, T };
	case 1:
		return { 1.0 - T, 0.0, 0.0, 1.0, 1.0, 1.0 };
	case 2:
		return { 0.0, 0.0, 1.0 - T, 1.0, 1.0, 1.0 };
	case 3:
		return { 0.0, 0.0, 0.0, T, 1.0, 1.0 };
	default:
		return { 0.0, 0.0, 0.0, 1.0, 1.0, 1.0 };
	}
}
inline CollisionShape DoorCollider(uint8_t _meta) {
	constexpr double T = 0.1875;
	CollisionShape s;
	switch (DoorState(_meta)) {
	case 0:
		s.Add({ 0.0, 0.0, 0.0, 1.0, 1.0, T });
		break;
	case 1:
		s.Add({ 1.0 - T, 0.0, 0.0, 1.0, 1.0, 1.0 });
		break;
	case 2:
		s.Add({ 0.0, 0.0, 1.0 - T, 1.0, 1.0, 1.0 });
		break;
	case 3:
		s.Add({ 0.0, 0.0, 0.0, T, 1.0, 1.0 });
		break;
	}
	return s;
}

// trapdoor
inline AABB TrapdoorAabb(uint8_t _meta) {
	constexpr double T = 0.1875;
	if (!(_meta & 4))
		return { 0.0, 0.0, 0.0, 1.0, T, 1.0 };
	switch (_meta & 3) {
	case 0:
		return { 0.0, 0.0, 1.0 - T, 1.0, 1.0, 1.0 };
	case 1:
		return { 0.0, 0.0, 0.0, 1.0, 1.0, T };
	case 2:
		return { 1.0 - T, 0.0, 0.0, 1.0, 1.0, 1.0 };
	case 3:
		return { 0.0, 0.0, 0.0, T, 1.0, 1.0 };
	default:
		return { 0.0, 0.0, 0.0, 1.0, 1.0, 1.0 };
	}
}

inline CollisionShape FarmlandCollider(uint8_t) {
	CollisionShape s;
	s.Add({ 0.0, 0.0, 0.0, 1.0, 0.9375, 1.0 });
	return s;
}

inline CollisionShape TrapdoorCollider(uint8_t _meta) {
	constexpr double T = 0.1875;
	CollisionShape s;
	if (!(_meta & 4)) {
		s.Add({ 0.0, 0.0, 0.0, 1.0, T, 1.0 });
		return s;
	}
	switch (_meta & 3) {
	case 0:
		s.Add({ 0.0, 0.0, 1.0 - T, 1.0, 1.0, 1.0 });
		break;
	case 1:
		s.Add({ 0.0, 0.0, 0.0, 1.0, 1.0, T });
		break;
	case 2:
		s.Add({ 1.0 - T, 0.0, 0.0, 1.0, 1.0, 1.0 });
		break;
	case 3:
		s.Add({ 0.0, 0.0, 0.0, T, 1.0, 1.0 });
		break;
	}
	return s;
}

// bed
inline AABB BedAabb(uint8_t) {
	return { 0.0, 0.0, 0.0, 1.0, 0.5625, 1.0 };
}
inline CollisionShape BedCollider(uint8_t) {
	CollisionShape s;
	s.Add({ 0.0, 0.0, 0.0, 1.0, 0.5625, 1.0 });
	return s;
}

// fence
inline CollisionShape FenceCollider(uint8_t) {
	CollisionShape s;
	s.Add({ 0.0, 0.0, 0.0, 1.0, 1.5, 1.0 });
	return s;
}

// cake
inline AABB CakeAabb(uint8_t _meta) {
	double x0 = (1 + _meta * 2) / 16.0;
	return { x0, 0.0, 0.0625, 1.0 - 0.0625, 0.5 - 0.0625, 1.0 - 0.0625 };
}
inline CollisionShape CakeCollider(uint8_t _meta) {
	double x0 = (1 + _meta * 2) / 16.0;
	CollisionShape s;
	s.Add({ x0, 0.0, 0.0625, 1.0 - 0.0625, 0.5 - 0.0625, 1.0 - 0.0625 });
	return s;
}

// repeater
inline AABB RepeaterAabb(uint8_t) {
	return { 0.0, 0.0, 0.0, 1.0, 0.125, 1.0 };
}
inline CollisionShape EmptyCollider(uint8_t) {
	return {};
}

// button
inline AABB ButtonAabb(uint8_t _meta) {
	const int face = _meta & 7;
	const bool pressed = (_meta & 8) != 0;
	constexpr double LO = 0.375, HI = 0.625, HW = 0.1875;
	const double depth = pressed ? 0.0625 : 0.125;
	switch (face) {
	case 1:
		return { 0.0, LO, 0.5 - HW, depth, HI, 0.5 + HW };
	case 2:
		return { 1.0 - depth, LO, 0.5 - HW, 1.0, HI, 0.5 + HW };
	case 3:
		return { 0.5 - HW, LO, 0.0, 0.5 + HW, HI, depth };
	case 4:
		return { 0.5 - HW, LO, 1.0 - depth, 0.5 + HW, HI, 1.0 };
	default:
		return {};
	}
}

// lever
inline AABB LeverAabb(uint8_t _meta) {
	constexpr double F = 0.1875;
	switch (_meta & 7) {
	case 1:
		return { 0.0, 0.2, 0.5 - F, F * 2.0, 0.8, 0.5 + F };
	case 2:
		return { 1.0 - F * 2.0, 0.2, 0.5 - F, 1.0, 0.8, 0.5 + F };
	case 3:
		return { 0.5 - F, 0.2, 0.0, 0.5 + F, 0.8, F * 2.0 };
	case 4:
		return { 0.5 - F, 0.2, 1.0 - F * 2.0, 0.5 + F, 0.8, 1.0 };
	default: {
		constexpr double G = 0.25;
		return { 0.5 - G, 0.0, 0.5 - G, 0.5 + G, 0.6, 0.5 + G };
	}
	}
}

// pressure plate
inline AABB PressurePlateAabb(uint8_t _meta) {
	constexpr double F = 0.0625;
	return { F, 0.0, F, 1.0 - F, (_meta == 1) ? 0.03125 : 0.0625, 1.0 - F };
}

// torch (normal + redstone, same box)
inline AABB TorchAabb(uint8_t _meta) {
	constexpr double F = 0.15;
	switch (_meta & 7) {
	case 1:
		return { 0.0, 0.2, 0.5 - F, F * 2.0, 0.8, 0.5 + F };
	case 2:
		return { 1.0 - F * 2.0, 0.2, 0.5 - F, 1.0, 0.8, 0.5 + F };
	case 3:
		return { 0.5 - F, 0.2, 0.0, 0.5 + F, 0.8, F * 2.0 };
	case 4:
		return { 0.5 - F, 0.2, 1.0 - F * 2.0, 0.5 + F, 0.8, 1.0 };
	default: {
		constexpr double G = 0.1;
		return { 0.5 - G, 0.0, 0.5 - G, 0.5 + G, 0.6, 0.5 + G };
	}
	}
}

// rail
inline AABB RailAabb(uint8_t) {
	return { 0.0, 0.0, 0.0, 1.0, 0.125, 1.0 };
}

// redstone dust
inline AABB RedstoneDustAabb(uint8_t) {
	return { 0.0, 0.0, 0.0, 1.0, 0.0625, 1.0 };
}

// farmland
// Collider is full cube; ray/selection use visual height 0.937
inline AABB FarmlandAabb(uint8_t) {
	return { 0.0, 0.0, 0.0, 1.0, 1.0, 1.0 };
}

// crop
inline AABB CropAabb(uint8_t) {
	return { 0.0, 0.0, 0.0, 1.0, 0.25, 1.0 }; // 4/16
}

// sapling / deadbush (f=0.4)
inline AABB SaplingAabb(uint8_t) {
	constexpr float F = 0.4f;
	return { 0.5f - F, 0.0f, 0.5f - F, 0.5f + F, F * 2.0f, 0.5f + F };
}

// tall grass
inline AABB TallGrassAabb(uint8_t) {
	constexpr float F = 0.4f;
	return { 0.5f - F, 0.0f, 0.5f - F, 0.5f + F, 0.8f, 0.5f + F };
}

// mushroom (f=0.2)
inline AABB MushroomAabb(uint8_t) {
	constexpr float F = 0.2f;
	return { 0.5f - F, 0.0f, 0.5f - F, 0.5f + F, F * 2.0f, 0.5f + F };
}

// plant / flower (rose, dandelion) (f=0.2, h=f*3)
inline AABB PlantAabb(uint8_t) {
	constexpr float F = 0.2f;
	return { 0.5f - F, 0.0f, 0.5f - F, 0.5f + F, F * 3.0f, 0.5f + F };
}

// sugarcane
inline AABB SugarcaneAabb(uint8_t) {
	constexpr float F = 0.375f;
	return { 0.5f - F, 0.0f, 0.5f - F, 0.5f + F, 1.0f, 0.5f + F };
}

// Liquids have no collision
inline AABB LiquidAabb(uint8_t) {
	return { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
}

// Soul sand is indented 1 block
inline CollisionShape SoulSandCollider(uint8_t) {
	CollisionShape collider;
	collider.Add({ 0.0, 0.0, 0.0, 1.0, 0.875, 1.0 });
	return collider;
}

// piston head
inline AABB PistonHeadAabb(uint8_t _meta) {
	switch (_meta & 7) {
	case 0:
		return { 0.0, 0.0, 0.0, 1.0, 0.25, 1.0 };
	case 1:
		return { 0.0, 0.75, 0.0, 1.0, 1.0, 1.0 };
	case 2:
		return { 0.0, 0.0, 0.0, 1.0, 1.0, 0.25 };
	case 3:
		return { 0.0, 0.0, 0.75, 1.0, 1.0, 1.0 };
	case 4:
		return { 0.0, 0.0, 0.0, 0.25, 1.0, 1.0 };
	case 5:
		return { 0.75, 0.0, 0.0, 1.0, 1.0, 1.0 };
	default:
		return { 0.0, 0.0, 0.0, 1.0, 1.0, 1.0 };
	}
}
inline CollisionShape PistonHeadCollider(uint8_t _meta) {
	CollisionShape s;
	switch (_meta & 7) {
	case 0:
		s.Add({ 0.0, 0.0, 0.0, 1.0, 0.25, 1.0 });
		s.Add({ 0.375, 0.25, 0.375, 0.625, 1.0, 0.625 });
		break;
	case 1:
		s.Add({ 0.0, 0.75, 0.0, 1.0, 1.0, 1.0 });
		s.Add({ 0.375, 0.0, 0.375, 0.625, 0.75, 0.625 });
		break;
	case 2:
		s.Add({ 0.0, 0.0, 0.0, 1.0, 1.0, 0.25 });
		s.Add({ 0.25, 0.375, 0.25, 0.75, 0.625, 1.0 });
		break;
	case 3:
		s.Add({ 0.0, 0.0, 0.75, 1.0, 1.0, 1.0 });
		s.Add({ 0.25, 0.375, 0.0, 0.75, 0.625, 0.75 });
		break;
	case 4:
		s.Add({ 0.0, 0.0, 0.0, 0.25, 1.0, 1.0 });
		s.Add({ 0.375, 0.25, 0.25, 0.625, 0.75, 1.0 });
		break;
	case 5:
		s.Add({ 0.75, 0.0, 0.0, 1.0, 1.0, 1.0 });
		s.Add({ 0.0, 0.375, 0.25, 0.75, 0.625, 0.75 });
		break;
	}
	return s;
}

// wall sign
inline AABB WallSignAabb(uint8_t _meta) {
	const auto dir = GetDirectionFromMeta(BLOCK_SIGN_WALL, _meta);
	constexpr double signBottom = 9.0 / 32.0;
	constexpr double signTop = 25.0 / 32.0;
	constexpr double wallOffset = 2.0 / 16.0;
	switch (dir) {
	case Cardinal::Value::North:
		return { 0.0, signBottom, 1.0 - wallOffset, 1.0, signTop, 1.0 };
	case Cardinal::Value::South:
		return { 0.0, signBottom, 0.0, 1.0, signTop, wallOffset };
	case Cardinal::Value::West:
		return { 1.0 - wallOffset, signBottom, 0.0, 1.0, signTop, 1.0 };
	case Cardinal::Value::East:
		return { 0.0, signBottom, 0.0, wallOffset, signTop, 1.0 };
	default:
		return {};
	}
}

// standing sign
inline AABB StandingSignAabb(uint8_t /*_meta*/) {
	return { 0.25F, 0.0F, 0.25F, 0.75F, 1.0F, 0.75F };
}
}; // namespace Blocks