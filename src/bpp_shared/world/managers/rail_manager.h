/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once
#include "blocks.h"
#include "blocks/block_builder.h"
#include "blocks/block_properties.h"
#include "direction.h"
#include "numeric_structs.h"
#include <array>
#include <cstdint>
#include <vector>

class WorldManager;
struct RailManager {
	// Awe shit, here we go again
	// Rails have a LOT of logic that goes on so
	// Separate file!! (block_behaviors.cpp is already MASSIVE)
	static Int3 FindRailConnection(WorldManager& _world, Int3 _pos, Cardinal::Value _dir);
	static std::vector<Cardinal::Value> GetVerifiedConnections(WorldManager& _world, Int3 _pos);
	static bool CanConnect(WorldManager& _world, Int3 _pos, Cardinal::Value _dir);
	static bool IsActuallyConnected(WorldManager& _world, Int3 _pos, Cardinal::Value _dir);
	static int GetAdjacentTrackCount(WorldManager& _world, Int3 _pos);
	static Blocks::RailShape DetermineRailShape(WorldManager& _world, Int3 _pos, BlockType _block);
	static Blocks::RailShape DetermineRailShapeWithAddition(WorldManager& _world, Int3 _pos, BlockType _block,
	                                                        Cardinal::Value _newDir);
	static bool IsRailPowered(WorldManager& _world, Int3 _pos);
	static void UpdateRailPower(WorldManager& _world, Int3 _pos, BlockType _block);

	static void RefreshRail(WorldManager& _world, Int3 _pos, BlockType _block, bool _forceWrite = true);

	static bool IsRail(BlockType _block) {
		return _block == BLOCK_RAIL || _block == BLOCK_RAIL_POWERED || _block == BLOCK_RAIL_DETECTOR;
	}

	static bool RailCanCurve(BlockType _block) {
		return _block == BLOCK_RAIL;
	}

	static Blocks::RailShape GetRailShape(uint8_t _meta, BlockType _block) {
		if (!IsRail(_block))
			return Blocks::RailShape::FlatNorthSouth;
		// Powered / Detector rails can't curve
		return static_cast<Blocks::RailShape>(_meta & (_block == BLOCK_RAIL ? 0xF : 0x7));
	}

	static std::array<Cardinal::Value, 2> GetImpliedConnections(Blocks::RailShape _shape) {
		// Name is a little confusing,
		// But this is just what possible connection points exist
		// Given the shape of the rail
		switch (_shape) {
		case Blocks::RailShape::FlatNorthSouth:
		case Blocks::RailShape::AscendingNorth:
		case Blocks::RailShape::AscendingSouth:
			return { Cardinal::Value::North, Cardinal::Value::South };
		case Blocks::RailShape::FlatEastWest:
		case Blocks::RailShape::AscendingEast:
		case Blocks::RailShape::AscendingWest:
			return { Cardinal::Value::East, Cardinal::Value::West };
		case Blocks::RailShape::CurveNorthEast:
			return { Cardinal::Value::North, Cardinal::Value::East };
		case Blocks::RailShape::CurveSouthEast:
			return { Cardinal::Value::South, Cardinal::Value::East };
		case Blocks::RailShape::CurveSouthWest:
			return { Cardinal::Value::South, Cardinal::Value::West };
		case Blocks::RailShape::CurveNorthWest:
			return { Cardinal::Value::North, Cardinal::Value::West };
		default:
			return {};
		}
	}
};