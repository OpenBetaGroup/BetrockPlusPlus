/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 * 
*/

// This disgusting code needs to exist because Notch couldn't ever make up his mind about
// which values should indicate the direction of a directional block.
// This is my best attempt at making a somewhat readable and "easy" way
// to normalize that bullshit and put it into a consistent format.
// Please, reuse this code as you need it, or improve it. Nobody should need to do this again!
//
// This has been made a lot less painful via the Beta Wiki, woohoo!
// http://localhost:5173/beta-wiki/general/blocks#direction-look-up
//
// Kind regards, Pixel Brush

#include "direction_fixer.h"

// Makes this a little more readable-ish?
enum DirectionBlockType : size_t {
	Torch,
	Lever,
	Button,
	Stairs,
	Doors,
	DispenserFurnaceLadderWallSign,
	Pistons,
	Bed,
	PumpkinJackOLatern,
	Trapdoor,
	RedstoneRepeater,
	Paintings,
	MAX_DIRECTION_BLOCK_TYPE
};

// Maps the metadata value to a direction value
constexpr Cardinal::Value META_TO_DIRECTION_LUT[MAX_DIRECTION_BLOCK_TYPE][0b111]{
	// Torches
	{ Cardinal::Value::None, Cardinal::Value::East, Cardinal::Value::West, Cardinal::Value::South,
	  Cardinal::Value::North, Cardinal::Value::Up, Cardinal::Value::None },
	// Levers
	{ Cardinal::Value::None, Cardinal::Value::East, Cardinal::Value::West, Cardinal::Value::South,
	  Cardinal::Value::North, Cardinal::Value::Up, Cardinal::Value::Up },
	// Buttons
	{ Cardinal::Value::None, Cardinal::Value::East, Cardinal::Value::West, Cardinal::Value::South,
	  Cardinal::Value::North, Cardinal::Value::None, Cardinal::Value::None },
	// Stairs
	{ Cardinal::Value::East, Cardinal::Value::West, Cardinal::Value::South, Cardinal::Value::North,
	  Cardinal::Value::None, Cardinal::Value::None, Cardinal::Value::None },
	// Doors
	{ Cardinal::Value::East, Cardinal::Value::South, Cardinal::Value::West, Cardinal::Value::North,
	  Cardinal::Value::None, Cardinal::Value::None, Cardinal::Value::None },
	// Dispenser/Furnace/Ladder/Wall Sign
	{ Cardinal::Value::None, Cardinal::Value::None, Cardinal::Value::North, Cardinal::Value::South,
	  Cardinal::Value::West, Cardinal::Value::East, Cardinal::Value::None },
	// Pistons
	{ Cardinal::Value::Down, Cardinal::Value::Up, Cardinal::Value::North, Cardinal::Value::South, Cardinal::Value::West,
	  Cardinal::Value::East, Cardinal::Value::None },
	// Beds
	{ Cardinal::Value::South, Cardinal::Value::West, Cardinal::Value::North, Cardinal::Value::East,
	  Cardinal::Value::None, Cardinal::Value::None, Cardinal::Value::None },
	// Pumpkins/Jack'o'Lantern
	{ Cardinal::Value::South, Cardinal::Value::West, Cardinal::Value::North, Cardinal::Value::East,
	  Cardinal::Value::None, Cardinal::Value::None, Cardinal::Value::None },
	// Trapdoors
	{ Cardinal::Value::South, Cardinal::Value::North, Cardinal::Value::East, Cardinal::Value::West,
	  Cardinal::Value::None, Cardinal::Value::None, Cardinal::Value::None },
	// Redstone Repeater
	{ Cardinal::Value::North, Cardinal::Value::East, Cardinal::Value::South, Cardinal::Value::West,
	  Cardinal::Value::None, Cardinal::Value::None, Cardinal::Value::None },
	// Paintings
	{ Cardinal::Value::North, Cardinal::Value::West, Cardinal::Value::South, Cardinal::Value::East,
	  Cardinal::Value::None, Cardinal::Value::None, Cardinal::Value::None },
};

/**
 * @brief Turn a metadata value into an appropriate direction for the block
 * 
 * @param _type Block type
 * @param _meta Metadata value
 * @return Cardinal::Value 
 */
Cardinal::Value GetDirectionFromMeta(const BlockType _type, const uint8_t _meta) {
	switch (_type) {
	case BLOCK_TORCH:
	case BLOCK_REDSTONE_TORCH_OFF:
	case BLOCK_REDSTONE_TORCH_ON:
		return META_TO_DIRECTION_LUT[DirectionBlockType::Torch][_meta & 0b111];
	case BLOCK_LEVER:
		// Note: Does not differentiate North-South/East-West direction
		return META_TO_DIRECTION_LUT[DirectionBlockType::Lever][_meta & 0b111];
	case BLOCK_BUTTON_STONE:
		return META_TO_DIRECTION_LUT[DirectionBlockType::Button][_meta & 0b111];
	case BLOCK_STAIRS_COBBLESTONE:
	case BLOCK_STAIRS_WOOD:
		return META_TO_DIRECTION_LUT[DirectionBlockType::Stairs][_meta & 0b11];
	case BLOCK_DOOR_IRON:
	case BLOCK_DOOR_WOOD:
		return META_TO_DIRECTION_LUT[DirectionBlockType::Doors][_meta & 0b11];
	case BLOCK_DISPENSER:
	case BLOCK_FURNACE:
	case BLOCK_FURNACE_LIT:
	case BLOCK_LADDER:
	case BLOCK_SIGN_WALL:
		return META_TO_DIRECTION_LUT[DirectionBlockType::DispenserFurnaceLadderWallSign][_meta & 0b111];
	case BLOCK_PISTON:
	case BLOCK_PISTON_STICKY:
	case BLOCK_PISTON_HEAD:
		// TODO: Check if piston moving uses this too?
		return META_TO_DIRECTION_LUT[DirectionBlockType::Pistons][_meta & 0b111];
	case BLOCK_BED:
		return META_TO_DIRECTION_LUT[DirectionBlockType::Bed][_meta & 0b11];
	case BLOCK_PUMPKIN:
	case BLOCK_PUMPKIN_LIT:
		return META_TO_DIRECTION_LUT[DirectionBlockType::PumpkinJackOLatern][_meta & 0b11];
	case BLOCK_TRAPDOOR:
		return META_TO_DIRECTION_LUT[DirectionBlockType::Trapdoor][_meta & 0b11];
	case BLOCK_REDSTONE_REPEATER_OFF:
	case BLOCK_REDSTONE_REPEATER_ON:
		return META_TO_DIRECTION_LUT[DirectionBlockType::RedstoneRepeater][_meta & 0b11];
	default: // Is a non-directional block
		return Cardinal::Value::None;
	}
}

/**
 * @brief Turn a direction into an appropriate metadata value for the block
 * 
 * @param _type Block type
 * @param _dir Cardinal::Value
 * @return Metadata value 
 */
uint8_t GetMetaFromDirection(const BlockType _type, const Cardinal::Value _dir) {
	switch (_type) {
	case BLOCK_TORCH:
	case BLOCK_REDSTONE_TORCH_OFF:
	case BLOCK_REDSTONE_TORCH_ON:
	case BLOCK_LEVER:
		switch (_dir) {
		case Cardinal::Value::North:
			return 4;
		case Cardinal::Value::South:
			return 3;
		case Cardinal::Value::East:
			return 1;
		case Cardinal::Value::West:
			return 2;
		// Note: For levers, handle North-South/East-West direction separately, defaults to North-South!
		case Cardinal::Value::Up:
			return 5;
		default:
			return 0;
		}
	case BLOCK_BUTTON_STONE:
		switch (_dir) {
		case Cardinal::Value::North:
			return 4;
		case Cardinal::Value::South:
			return 3;
		case Cardinal::Value::East:
			return 1;
		case Cardinal::Value::West:
			return 2;
		default:
			return 0;
		}
	case BLOCK_STAIRS_COBBLESTONE:
	case BLOCK_STAIRS_WOOD:
		switch (_dir) {
		case Cardinal::Value::North:
			return 3;
		case Cardinal::Value::South:
			return 2;
		case Cardinal::Value::East:
			return 0;
		case Cardinal::Value::West:
			return 1;
		default:
			return 0;
		}
	case BLOCK_DOOR_IRON:
	case BLOCK_DOOR_WOOD:
		switch (_dir) {
		case Cardinal::Value::North:
			return 3;
		case Cardinal::Value::South:
			return 1;
		case Cardinal::Value::East:
			return 0;
		case Cardinal::Value::West:
			return 2;
		default:
			return 0;
		}
	case BLOCK_DISPENSER:
	case BLOCK_FURNACE:
	case BLOCK_FURNACE_LIT:
	case BLOCK_LADDER:
	case BLOCK_SIGN_WALL:
		switch (_dir) {
		case Cardinal::Value::North:
			return 2;
		case Cardinal::Value::South:
			return 3;
		case Cardinal::Value::East:
			return 5;
		case Cardinal::Value::West:
			return 4;
		default:
			return 0;
		}
	case BLOCK_PISTON:
	case BLOCK_PISTON_STICKY:
	case BLOCK_PISTON_HEAD:
		// TODO: Check if piston moving uses this too?
		switch (_dir) {
		case Cardinal::Value::North:
			return 2;
		case Cardinal::Value::South:
			return 3;
		case Cardinal::Value::West:
			return 4;
		case Cardinal::Value::East:
			return 5;
		case Cardinal::Value::Up:
			return 1;
		case Cardinal::Value::Down:
			return 0;
		default:
			return 0;
		}
	case BLOCK_BED:
		switch (_dir) {
		case Cardinal::Value::North:
			return 2;
		case Cardinal::Value::South:
			return 0;
		case Cardinal::Value::East:
			return 3;
		case Cardinal::Value::West:
			return 1;
		default:
			return 0;
		}
	case BLOCK_PUMPKIN:
	case BLOCK_PUMPKIN_LIT:
		switch (_dir) {
		case Cardinal::Value::North:
			return 2;
		case Cardinal::Value::South:
			return 0;
		case Cardinal::Value::East:
			return 3;
		case Cardinal::Value::West:
			return 1;
		default:
			return 0;
		}
	case BLOCK_TRAPDOOR:
		switch (_dir) {
		case Cardinal::Value::North:
			return 1;
		case Cardinal::Value::South:
			return 0;
		case Cardinal::Value::East:
			return 2;
		case Cardinal::Value::West:
			return 3;
		default:
			return 0;
		}
	case BLOCK_REDSTONE_REPEATER_OFF:
	case BLOCK_REDSTONE_REPEATER_ON:
		switch (_dir) {
		case Cardinal::Value::North:
			return 0;
		case Cardinal::Value::South:
			return 2;
		case Cardinal::Value::East:
			return 1;
		case Cardinal::Value::West:
			return 3;
		default:
			return 0;
		}
	default: // Block without directional data
		return 0;
	}
}

Cardinal::Value FromPaintingDirectionToDirection(const PacketData::PaintingDirection _value) {
	if (_value > PacketData::PaintingDirection::X_PLUS)
		return Cardinal::Value::None;
	return META_TO_DIRECTION_LUT[Paintings][static_cast<size_t>(_value)];
}

PacketData::PaintingDirection FromDirectionToPaintingDirection(const Cardinal::Value _dir) {
	switch (_dir) {
	default:
	case Cardinal::Value::North:
		return PacketData::PaintingDirection::Z_MINUS;
	case Cardinal::Value::South:
		return PacketData::PaintingDirection::Z_PLUS;
	case Cardinal::Value::East:
		return PacketData::PaintingDirection::X_PLUS;
	case Cardinal::Value::West:
		return PacketData::PaintingDirection::X_MINUS;
	}
}

Cardinal::Value FaceDirectionToDirection(const PacketData::FaceDirection _face) {
	switch (_face) {
	case PacketData::FaceDirection::Z_MINUS:
		return Cardinal::Value::North;
	case PacketData::FaceDirection::Z_PLUS:
		return Cardinal::Value::South;
	case PacketData::FaceDirection::X_MINUS:
		return Cardinal::Value::West;
	case PacketData::FaceDirection::X_PLUS:
		return Cardinal::Value::East;
	case PacketData::FaceDirection::Y_MINUS:
		return Cardinal::Value::Down;
	case PacketData::FaceDirection::Y_PLUS:
		return Cardinal::Value::Up;
	default:
		return Cardinal::Value::None;
	}
}

PacketData::FaceDirection DirectionToFaceDirection(const Cardinal::Value _dir) {
	switch (_dir) {
	case Cardinal::Value::North:
		return PacketData::FaceDirection::Z_MINUS;
	case Cardinal::Value::South:
		return PacketData::FaceDirection::Z_PLUS;
	case Cardinal::Value::West:
		return PacketData::FaceDirection::X_MINUS;
	case Cardinal::Value::East:
		return PacketData::FaceDirection::X_PLUS;
	case Cardinal::Value::Down:
		return PacketData::FaceDirection::Y_MINUS;
	case Cardinal::Value::Up:
		return PacketData::FaceDirection::Y_PLUS;
	default:
		return PacketData::FaceDirection::INVALID_USE;
	}
}