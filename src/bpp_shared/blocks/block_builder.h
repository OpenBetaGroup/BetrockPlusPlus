/*
 * Copyright (c) 2026, mudkipdev <github.com/mudkipdev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once
#include "../direction.h"
#include "base_structs.h"
#include "blocks.h"
#include <algorithm>
#include <cstdint>

namespace Blocks {
enum class WoodType : uint8_t {
	Oak = 0,
	Spruce = 1,
	Birch = 2,
};

enum class WoolColor : uint8_t {
	White = 0,
	Orange = 1,
	Magenta = 2,
	LightBlue = 3,
	Yellow = 4,
	Lime = 5,
	Pink = 6,
	Gray = 7,
	LightGray = 8,
	Cyan = 9,
	Purple = 10,
	Blue = 11,
	Brown = 12,
	Green = 13,
	Red = 14,
	Black = 15,
};

enum class SlabType : uint8_t {
	Stone = 0,
	Sandstone = 1,
	Wood = 2,
	Cobblestone = 3,
};

enum class TallGrassType : uint8_t {
	DeadBush = 0,
	TallGrass = 1,
	Fern = 2,
};

enum class TorchAttachment : uint8_t {
	WestWall = 1,
	EastWall = 2,
	NorthWall = 3,
	SouthWall = 4,
	Floor = 5,
};

enum class WallFacing : uint8_t {
	North = 2,
	South = 3,
	West = 4,
	East = 5,
};

enum class LeverMount : uint8_t {
	WestWall = 1,
	EastWall = 2,
	NorthWall = 3,
	SouthWall = 4,
	FloorEastWest = 5,
	FloorNorthSouth = 6,
};

enum class ButtonMount : uint8_t {
	WestWall = 1,
	EastWall = 2,
	NorthWall = 3,
	SouthWall = 4,
};

enum class RailShape : int8_t {
	INVALID = -1,
	FlatNorthSouth = 0,
	FlatEastWest = 1,
	AscendingEast = 2,
	AscendingWest = 3,
	AscendingNorth = 4,
	AscendingSouth = 5,
	CurveSouthEast = 6,
	CurveSouthWest = 7,
	CurveNorthWest = 8,
	CurveNorthEast = 9,
};

enum class PistonFacing : uint8_t {
	Down = 0,
	Up = 1,
	North = 2,
	South = 3,
	West = 4,
	East = 5,
};

enum class StairsDirection : uint8_t {
	East = 0,
	West = 1,
	South = 2,
	North = 3,
};

struct StairsBuilder {
	constexpr explicit StairsBuilder(BlockType _id) : id(_id) {}

	StairsBuilder facing(StairsDirection _direction) const {
		StairsBuilder b = *this;
		b.direction = _direction;
		return b;
	}

	Block asBlock() const {
		return Block{ id, static_cast<uint8_t>(direction) };
	}
	operator Block() const {
		return asBlock();
	}

private:
	BlockType id;
	StairsDirection direction = StairsDirection::East;
};

struct FluidBuilder {
	constexpr explicit FluidBuilder(BlockType _id) : id(_id) {}

	FluidBuilder level(uint8_t _level) const {
		FluidBuilder b = *this;
		b.level_ = std::min<uint8_t>(_level, 7);
		return b;
	}
	FluidBuilder falling(bool _falling) const {
		FluidBuilder b = *this;
		b.falling_ = _falling;
		return b;
	}

	Block asBlock() const {
		uint8_t data = level_ & 0x7;
		if (falling_)
			data |= 0x8;
		return Block{ id, data };
	}
	operator Block() const {
		return asBlock();
	}

private:
	BlockType id;
	uint8_t level_ = 0;
	bool falling_ = false;
};

struct SaplingBuilder {
	SaplingBuilder type(WoodType _type) const {
		SaplingBuilder b = *this;
		b.type_ = _type;
		return b;
	}
	SaplingBuilder readyToGrow(bool _readyToGrow) const {
		SaplingBuilder b = *this;
		b.readyToGrow_ = _readyToGrow;
		return b;
	}

	Block asBlock() const {
		uint8_t data = static_cast<uint8_t>(type_);
		if (readyToGrow_)
			data |= 0x8;
		return Block{ id, data };
	}
	operator Block() const {
		return asBlock();
	}

private:
	BlockType id = BLOCK_SAPLING;
	WoodType type_ = WoodType::Oak;
	bool readyToGrow_ = false;
};

struct LogBuilder {
	LogBuilder type(WoodType _type) const {
		LogBuilder b = *this;
		b.type_ = _type;
		return b;
	}

	Block asBlock() const {
		return Block{ id, static_cast<uint8_t>(type_) };
	}
	operator Block() const {
		return asBlock();
	}

private:
	BlockType id = BLOCK_LOG;
	WoodType type_ = WoodType::Oak;
};

struct LeavesBuilder {
	LeavesBuilder type(WoodType _type) const {
		LeavesBuilder b = *this;
		b.type_ = _type;
		return b;
	}
	LeavesBuilder decaying(bool _decaying) const {
		LeavesBuilder b = *this;
		b.decaying_ = _decaying;
		return b;
	}

	Block asBlock() const {
		uint8_t data = static_cast<uint8_t>(type_);
		if (decaying_)
			data |= 0x8;
		return Block{ id, data };
	}
	operator Block() const {
		return asBlock();
	}

private:
	BlockType id = BLOCK_LEAVES;
	WoodType type_ = WoodType::Oak;
	bool decaying_ = false;
};

struct TallGrassBuilder {
	TallGrassBuilder type(TallGrassType _type) const {
		TallGrassBuilder b = *this;
		b.type_ = _type;
		return b;
	}

	Block asBlock() const {
		return Block{ id, static_cast<uint8_t>(type_) };
	}
	operator Block() const {
		return asBlock();
	}

private:
	BlockType id = BLOCK_TALLGRASS;
	TallGrassType type_ = TallGrassType::TallGrass;
};

struct WoolBuilder {
	WoolBuilder color(WoolColor _color) const {
		WoolBuilder b = *this;
		b.color_ = _color;
		return b;
	}

	Block asBlock() const {
		return Block{ id, static_cast<uint8_t>(color_) };
	}
	operator Block() const {
		return asBlock();
	}

private:
	BlockType id = BLOCK_WOOL;
	WoolColor color_ = WoolColor::White;
};

struct SlabBuilder {
	constexpr explicit SlabBuilder(BlockType _id) : id(_id) {}

	SlabBuilder type(SlabType _type) const {
		SlabBuilder b = *this;
		b.type_ = _type;
		return b;
	}

	Block asBlock() const {
		return Block{ id, static_cast<uint8_t>(type_) };
	}
	operator Block() const {
		return asBlock();
	}

private:
	BlockType id;
	SlabType type_ = SlabType::Stone;
};

struct TorchBuilder {
	constexpr explicit TorchBuilder(BlockType _id) : id(_id) {}

	TorchBuilder attachment(TorchAttachment _attachment) const {
		TorchBuilder b = *this;
		b.attachment_ = _attachment;
		return b;
	}

	Block asBlock() const {
		return Block{ id, static_cast<uint8_t>(attachment_) };
	}
	operator Block() const {
		return asBlock();
	}

private:
	BlockType id;
	TorchAttachment attachment_ = TorchAttachment::Floor;
};

struct WallFacingBuilder {
	constexpr explicit WallFacingBuilder(BlockType _id) : id(_id) {}

	WallFacingBuilder facing(WallFacing _facing) const {
		WallFacingBuilder b = *this;
		b.facing_ = _facing;
		return b;
	}

	Block asBlock() const {
		return Block{ id, static_cast<uint8_t>(facing_) };
	}
	operator Block() const {
		return asBlock();
	}

private:
	BlockType id;
	WallFacing facing_ = WallFacing::North;
};

struct BedBuilder {
	BedBuilder facing(Cardinal::Value _direction) const {
		BedBuilder b = *this;
		b.direction = _direction;
		return b;
	}
	BedBuilder occupied(bool _occupied) const {
		BedBuilder b = *this;
		b.occupied_ = _occupied;
		return b;
	}
	BedBuilder head(bool _head) const {
		BedBuilder b = *this;
		b.head_ = _head;
		return b;
	}

	Block asBlock() const {
		uint8_t data;
		switch (direction) {
		case Cardinal::Value::South:
		default:
			data = 0;
			break;
		case Cardinal::Value::West:
			data = 1;
			break;
		case Cardinal::Value::North:
			data = 2;
			break;
		case Cardinal::Value::East:
			data = 3;
			break;
		}
		if (occupied_)
			data |= 0x4;
		if (head_)
			data |= 0x8;
		return Block{ id, data };
	}
	operator Block() const {
		return asBlock();
	}

private:
	BlockType id = BLOCK_BED;
	Cardinal::Value direction = Cardinal::Value::South;
	bool occupied_ = false;
	bool head_ = false;
};

struct PoweredRailBuilder {
	constexpr explicit PoweredRailBuilder(BlockType _id) : id(_id) {}

	PoweredRailBuilder shape(RailShape _shape) const {
		PoweredRailBuilder b = *this;
		b.shape_ = _shape;
		return b;
	}
	PoweredRailBuilder powered(bool _powered) const {
		PoweredRailBuilder b = *this;
		b.powered_ = _powered;
		return b;
	}

	Block asBlock() const {
		uint8_t data = static_cast<uint8_t>(shape_) & 0x7;
		if (powered_)
			data |= 0x8;
		return Block{ id, data };
	}
	operator Block() const {
		return asBlock();
	}

private:
	BlockType id;
	RailShape shape_ = RailShape::FlatNorthSouth;
	bool powered_ = false;
};

struct PistonBuilder {
	constexpr explicit PistonBuilder(BlockType _id) : id(_id) {}

	PistonBuilder facing(PistonFacing _facing) const {
		PistonBuilder b = *this;
		b.facing_ = _facing;
		return b;
	}
	PistonBuilder extended(bool _extended) const {
		PistonBuilder b = *this;
		b.extended_ = _extended;
		return b;
	}

	Block asBlock() const {
		uint8_t data = static_cast<uint8_t>(facing_);
		if (extended_)
			data |= 0x8;
		return Block{ id, data };
	}
	operator Block() const {
		return asBlock();
	}

private:
	BlockType id;
	PistonFacing facing_ = PistonFacing::Up;
	bool extended_ = false;
};

struct PistonHeadBuilder {
	PistonHeadBuilder facing(PistonFacing _facing) const {
		PistonHeadBuilder b = *this;
		b.facing_ = _facing;
		return b;
	}
	PistonHeadBuilder sticky(bool _sticky) const {
		PistonHeadBuilder b = *this;
		b.sticky_ = _sticky;
		return b;
	}

	Block asBlock() const {
		uint8_t data = static_cast<uint8_t>(facing_);
		if (sticky_)
			data |= 0x8;
		return Block{ id, data };
	}
	operator Block() const {
		return asBlock();
	}

private:
	BlockType id = BLOCK_PISTON_HEAD;
	PistonFacing facing_ = PistonFacing::Up;
	bool sticky_ = false;
};

struct DoorBuilder {
	constexpr explicit DoorBuilder(BlockType _id) : id(_id) {}

	DoorBuilder rotation(uint8_t _rotation) const {
		DoorBuilder b = *this;
		b.rotation_ = _rotation & 0x3;
		return b;
	}
	DoorBuilder open(bool _open) const {
		DoorBuilder b = *this;
		b.open_ = _open;
		return b;
	}
	DoorBuilder upperHalf(bool _upperHalf) const {
		DoorBuilder b = *this;
		b.upperHalf_ = _upperHalf;
		return b;
	}

	Block asBlock() const {
		uint8_t data = rotation_ & 0x3;
		if (open_)
			data |= 0x4;
		if (upperHalf_)
			data |= 0x8;
		return Block{ id, data };
	}
	operator Block() const {
		return asBlock();
	}

private:
	BlockType id;
	uint8_t rotation_ = 0;
	bool open_ = false;
	bool upperHalf_ = false;
};

struct SignBuilder {
	SignBuilder rotation(uint8_t _rotation) const {
		SignBuilder b = *this;
		b.rotation_ = _rotation & 0xF;
		return b;
	}

	Block asBlock() const {
		return Block{ id, static_cast<uint8_t>(rotation_ & 0xF) };
	}
	operator Block() const {
		return asBlock();
	}

private:
	BlockType id = BLOCK_SIGN_STANDING;
	uint8_t rotation_ = 0;
};

struct RailBuilder {
	RailBuilder shape(RailShape _shape) const {
		RailBuilder b = *this;
		b.shape_ = _shape;
		return b;
	}

	Block asBlock() const {
		return Block{ id, static_cast<uint8_t>(shape_) };
	}
	operator Block() const {
		return asBlock();
	}

private:
	BlockType id = BLOCK_RAIL;
	RailShape shape_ = RailShape::FlatNorthSouth;
};

struct LeverBuilder {
	LeverBuilder mount(LeverMount _mount) const {
		LeverBuilder b = *this;
		b.mount_ = _mount;
		return b;
	}
	LeverBuilder on(bool _on) const {
		LeverBuilder b = *this;
		b.on_ = _on;
		return b;
	}

	Block asBlock() const {
		uint8_t data = static_cast<uint8_t>(mount_);
		if (on_)
			data |= 0x8;
		return Block{ id, data };
	}
	operator Block() const {
		return asBlock();
	}

private:
	BlockType id = BLOCK_LEVER;
	LeverMount mount_ = LeverMount::FloorEastWest;
	bool on_ = false;
};

struct ButtonBuilder {
	ButtonBuilder mount(ButtonMount _mount) const {
		ButtonBuilder b = *this;
		b.mount_ = _mount;
		return b;
	}
	ButtonBuilder pressed(bool _pressed) const {
		ButtonBuilder b = *this;
		b.pressed_ = _pressed;
		return b;
	}

	Block asBlock() const {
		uint8_t data = static_cast<uint8_t>(mount_);
		if (pressed_)
			data |= 0x8;
		return Block{ id, data };
	}
	operator Block() const {
		return asBlock();
	}

private:
	BlockType id = BLOCK_BUTTON_STONE;
	ButtonMount mount_ = ButtonMount::WestWall;
	bool pressed_ = false;
};

struct SnowLayerBuilder {
	SnowLayerBuilder height(uint8_t _height) const {
		SnowLayerBuilder b = *this;
		b.height_ = std::min<uint8_t>(_height, 7);
		return b;
	}

	Block asBlock() const {
		return Block{ id, static_cast<uint8_t>(height_ & 0x7) };
	}
	operator Block() const {
		return asBlock();
	}

private:
	BlockType id = BLOCK_SNOW_LAYER;
	uint8_t height_ = 0;
};

struct TrapdoorBuilder {
	TrapdoorBuilder facing(Cardinal::Value _direction) const {
		TrapdoorBuilder b = *this;
		b.direction = _direction;
		return b;
	}
	TrapdoorBuilder open(bool _open) const {
		TrapdoorBuilder b = *this;
		b.open_ = _open;
		return b;
	}

	Block asBlock() const {
		uint8_t data;
		switch (direction) {
		case Cardinal::Value::East:
		default:
			data = 0;
			break;
		case Cardinal::Value::West:
			data = 1;
			break;
		case Cardinal::Value::North:
			data = 2;
			break;
		case Cardinal::Value::South:
			data = 3;
			break;
		}
		if (open_)
			data |= 0x4;
		return Block{ id, data };
	}
	operator Block() const {
		return asBlock();
	}

private:
	BlockType id = BLOCK_TRAPDOOR;
	Cardinal::Value direction = Cardinal::Value::South;
	bool open_ = false;
};

struct PumpkinBuilder {
	constexpr explicit PumpkinBuilder(BlockType _id) : id(_id) {}

	PumpkinBuilder facing(Cardinal::Value _direction) const {
		PumpkinBuilder b = *this;
		b.direction = _direction;
		return b;
	}

	Block asBlock() const {
		uint8_t data;
		switch (direction) {
		case Cardinal::Value::South:
		default:
			data = 0;
			break;
		case Cardinal::Value::West:
			data = 1;
			break;
		case Cardinal::Value::North:
			data = 2;
			break;
		case Cardinal::Value::East:
			data = 3;
			break;
		}
		return Block{ id, data };
	}
	operator Block() const {
		return asBlock();
	}

private:
	BlockType id;
	Cardinal::Value direction = Cardinal::Value::South;
};

struct RepeaterBuilder {
	constexpr explicit RepeaterBuilder(BlockType _id) : id(_id) {}

	RepeaterBuilder facing(Cardinal::Value _direction) const {
		RepeaterBuilder b = *this;
		b.direction = _direction;
		return b;
	}
	RepeaterBuilder delay(uint8_t _delay) const {
		RepeaterBuilder b = *this;
		b.delay_ = std::min<uint8_t>(_delay, 3);
		return b;
	}

	int delayTicks() const {
		return (static_cast<int>(delay_) + 1) * 2;
	}

	Block asBlock() const {
		uint8_t data;
		switch (direction) {
		case Cardinal::Value::North:
		default:
			data = 0;
			break;
		case Cardinal::Value::East:
			data = 1;
			break;
		case Cardinal::Value::South:
			data = 2;
			break;
		case Cardinal::Value::West:
			data = 3;
			break;
		}
		data |= (delay_ & 0x3) << 2;
		return Block{ id, data };
	}
	operator Block() const {
		return asBlock();
	}

private:
	BlockType id;
	Cardinal::Value direction = Cardinal::Value::North;
	uint8_t delay_ = 0;
};

inline constexpr FluidBuilder flowingWater{ BLOCK_WATER_FLOWING };
inline constexpr FluidBuilder water{ BLOCK_WATER_STILL };
inline constexpr FluidBuilder flowingLava{ BLOCK_LAVA_FLOWING };
inline constexpr FluidBuilder lava{ BLOCK_LAVA_STILL };
inline constexpr SaplingBuilder sapling{};
inline constexpr LogBuilder log{};
inline constexpr LeavesBuilder leaves{};
inline constexpr WallFacingBuilder dispenser{ BLOCK_DISPENSER };
inline constexpr BedBuilder bed{};
inline constexpr PoweredRailBuilder poweredRail{ BLOCK_RAIL_POWERED };
inline constexpr PoweredRailBuilder detectorRail{ BLOCK_RAIL_DETECTOR };
inline constexpr PistonBuilder stickyPiston{ BLOCK_PISTON_STICKY };
inline constexpr TallGrassBuilder tallGrass{};
inline constexpr PistonBuilder piston{ BLOCK_PISTON };
inline constexpr PistonHeadBuilder pistonHead{};
inline constexpr WoolBuilder wool{};
inline constexpr SlabBuilder doubleSlab{ BLOCK_DOUBLE_SLAB };
inline constexpr SlabBuilder slab{ BLOCK_SLAB };
inline constexpr TorchBuilder torch{ BLOCK_TORCH };
inline constexpr StairsBuilder woodenStairs{ BLOCK_STAIRS_WOOD };
inline constexpr WallFacingBuilder furnace{ BLOCK_FURNACE };
inline constexpr WallFacingBuilder litFurnace{ BLOCK_FURNACE_LIT };
inline constexpr SignBuilder sign{};
inline constexpr DoorBuilder woodenDoor{ BLOCK_DOOR_WOOD };
inline constexpr WallFacingBuilder ladder{ BLOCK_LADDER };
inline constexpr RailBuilder rail{};
inline constexpr StairsBuilder cobblestoneStairs{ BLOCK_STAIRS_COBBLESTONE };
inline constexpr WallFacingBuilder wallSign{ BLOCK_SIGN_WALL };
inline constexpr LeverBuilder lever{};
inline constexpr DoorBuilder ironDoor{ BLOCK_DOOR_IRON };
inline constexpr TorchBuilder redstoneTorch{ BLOCK_REDSTONE_TORCH_OFF };
inline constexpr TorchBuilder litRedstoneTorch{ BLOCK_REDSTONE_TORCH_ON };
inline constexpr ButtonBuilder stoneButton{};
inline constexpr SnowLayerBuilder snowLayer{};
inline constexpr PumpkinBuilder pumpkin{ BLOCK_PUMPKIN };
inline constexpr PumpkinBuilder jackOLantern{ BLOCK_PUMPKIN_LIT };
inline constexpr RepeaterBuilder redstoneRepeater{ BLOCK_REDSTONE_REPEATER_OFF };
inline constexpr RepeaterBuilder litRedstoneRepeater{ BLOCK_REDSTONE_REPEATER_ON };
inline constexpr TrapdoorBuilder trapdoor{};
} // namespace Blocks