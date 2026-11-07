/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 * 
*/

#pragma once
#include "inventory/item_stack.h"
#include "numeric_structs.h"
#include <cstdint>
#include <string>
#include <variant>

namespace PacketData {
// Used by the Mine Block Packet (0x0E)
enum MineStatus : uint8_t {
	DIGGING_STARTED = 0,
	DIGGING_FINISHED = 2,
	DROPPED_ITEM = 4
};

// Used by Mine and Place Block Packets (0x0E and 0x0F)
enum class FaceDirection : int8_t {
	INVALID_USE = -1,
	Y_MINUS = 0,
	Y_PLUS = 1,
	Z_MINUS = 2,
	Z_PLUS = 3,
	X_MINUS = 4,
	X_PLUS = 5
};

// Used by the Interact with Block Packet (0x11)
enum BlockInteraction : uint8_t {
	SLEEPING = 0
};

// Used by the Animation Packet (0x12)
enum Animation : int8_t {
	NONE = 0,
	// The player swings their arm (e.g. when attacking or using an item)
	PUNCH = 1,
	// While this still works as intended in b1.7.3,
	// the server does not send it.
	// Instead Entity Event (0x26) is used
	DAMAGE = 2,
	// The player is forced to leave the bed
	LEAVE_BED = 3,
	// An animation that seems to no longer
	// have any connected functionality in b1.7.3
	UNUSED = 4
};

// Used by the Player Action Packet (0x13)
enum PlayerAction : int8_t {
	START_SNEAKING = 1,
	STOP_SNEAKING = 2,
	STOP_SLEEPING = 3
};

// Used by the Spawn Object Packet (0x17)
enum ObjectType : int8_t {
	BOAT = 1,
	MINECART = 10,
	STORAGE_MINECART = 11,
	FURNACE_MINECART = 12,
	LIT_TNT = 50,
	ARROW = 60,
	THROWN_SNOWBALL = 61,
	THROWN_EGG = 62,
	FIREBALL = 63,
	FALLING_SAND = 70,
	FALLING_GRAVEL = 71,
	FISHING_BOBBER = 90
};

// Used by the Spawn Mob Packet (0x18)
enum MobType : int8_t {
	CREEPER = 50,
	SKELETON = 51,
	SPIDER = 52,
	GIANT_ZOMBIE = 53,
	ZOMBIE = 54,
	SLIME = 55,
	GHAST = 56,
	ZOMBIE_PIGMAN = 57,
	PIG = 90,
	SHEEP = 91,
	COW = 92,
	CHICKEN = 93,
	SQUID = 94,
	WOLF = 95
};

// Used by the Spawn Painting Packet (0x19)
enum class PaintingDirection : int32_t {
	Z_MINUS = 0,
	X_MINUS = 1,
	Z_PLUS = 2,
	X_PLUS = 3
};

// Used by the Entity Event Packet (0x26)
enum EntityEvent : int8_t {
	HURT = 2,
	DEATH = 3,
	// Wolf specific events
	SMOKE_PARTICLES = 6,
	HEART_PARTICLES = 7,
	START_SHAKING = 8
};

// Only used for the Entity Metadata pseudo-VM
class EntityMetadata {
public:
	static constexpr uint8_t END = 0x7F;
	enum Type : uint8_t {
		BYTE = 0,
		SHORT = 1,
		INTEGER = 2,
		FLOAT = 3,
		STRING = 4,
		ITEM = 5,
		COORDINATES = 6
	};

	enum Flags : uint8_t {
		BURNING = 0,
		SNEAKING = 1,
		RIDING = 2
	};
	struct DataEntry {
		Type type;
		uint8_t index;

		using Value = std::variant<int8_t, int16_t, int32_t, float, std::string, ItemStack, Int32_3>;

		Value value;
	};
};

// Used by Block Action Packet (0x36)
enum NoteInstrument : int8_t {
	HARP = 0,
	BASS_DRUM = 1,
	SNARE_DRUM = 2,
	HI_HAT = 3,
	BASS = 4,
};

enum NotePitch : int8_t {
	LOW_F_SHARP = 0,
	LOW_G = 1,
	LOW_G_SHARP = 2,
	LOW_A = 3,
	LOW_A_SHARP = 4,
	LOW_B = 5,
	LOW_C = 6,
	LOW_C_SHARP = 7,
	LOW_D = 8,
	LOW_D_SHARP = 9,
	LOW_E = 10,
	LOW_F = 11,
	HIGH_F_SHARP = 12,
	HIGH_G = 13,
	HIGH_G_SHARP = 14,
	HIGH_A = 15,
	HIGH_A_SHARP = 16,
	HIGH_B = 17,
	HIGH_C = 18,
	HIGH_C_SHARP = 19,
	HIGH_D = 20,
	HIGH_D_SHARP = 21,
	HIGH_E = 22,
	HIGH_F = 23,
	LAST_F_SHARP = 24
};

enum PistonState : int8_t {
	EXTEND = 0,
	RETRACT = 1
};

enum PistonDirection : int8_t {
	Y_MINUS = 0,
	Y_PLUS = 1,
	X_MINUS = 2,
	X_PLUS = 3,
	Z_MINUS = 4,
	Z_PLUS = 5
};

// Used by the World Event Packet (0x3D)
enum WorldEvent : int32_t {
	// Dispenser Click (Empty)
	DISPENSER_CLICK_EMPTY = 1000,
	// Dispenser Click
	DISPENSER_CLICK = 1001,
	// Bow shooting sound
	DISPENSER_ARROW_FIRE = 1002,
	// Door opening/closing sound
	DOOR_TOGGLE = 1003,
	// Extinguish fire sound
	FIRE_EXTINGUISH = 1004,
	// Record playing sound, requires music disc item id as parameter
	RECORD_PLAY = 1005,
	// Dispenser Puff effect, requires index for a position
	DISPENSER_PUFF = 2000,
	// Block breaking particle effect, requires block id
	BLOCK_BREAK = 2001
};

// Used by Game Event Packet (0x46)
enum GameEvent : int8_t {
	INVALID_BED = 0,
	START_RAINING = 1,
	STOP_RAINING = 2
};

// Used by Open Container (0x64)
enum WindowType : int8_t {
	CHEST = 0,
	CRAFTING_TABLE = 1,
	FURNACE = 2,
	DISPENSER = 3
};

// Used by Container Data (0x69)
enum ContainerDataType : int16_t {
	SMELTING_PROGRESS = 0,
	FUEL_REMAINING = 1,
	FUEL_DURATION = 2
};

enum MapDataType : int8_t {
	GRAPHICS = 0x00,
	ICON = 0x01
};
}; // namespace PacketData
