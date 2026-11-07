/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "rail_manager.h"
#include "redstone_manager.h"
#include "world.h"
#include <algorithm>

Int3 RailManager::FindRailConnection(WorldManager& _world, Int3 _pos, Cardinal::Value _dir) {
	// Order here matters!
	Int3 base = _pos.WithOffset(_dir);
	Cardinal::Value offsets[3] = { Cardinal::Value::None, Cardinal::Value::Up, Cardinal::Value::Down };
	for (size_t i = 0; i < 3; i++) {
		Int3 checkPos = base.WithOffset(offsets[i]);
		if (RailManager::IsRail(_world.GetBlockId(checkPos)))
			return checkPos;
	}
	return _pos; // Return our own pos if nothing is found
}

// TODO: Could this be turned into an array too?
std::vector<Cardinal::Value> RailManager::GetVerifiedConnections(WorldManager& _world, Int3 _pos) {
	auto shape = GetRailShape(_world.GetMetadata(_pos), _world.GetBlockId(_pos));
	auto implied = GetImpliedConnections(shape);

	std::vector<Cardinal::Value> verified;
	for (auto dir : implied) {
		Int3 farPos = FindRailConnection(_world, _pos, dir);
		if (farPos == _pos)
			continue; // nothing actually there

		// Does the far rail's OWN shape point back at _pos?
		auto farShape = GetRailShape(_world.GetMetadata(farPos), _world.GetBlockId(farPos));
		bool pointsBack = false;
		for (auto farDir : GetImpliedConnections(farShape)) {
			if (FindRailConnection(_world, farPos, farDir) == _pos) {
				pointsBack = true;
				break;
			}
		}
		if (pointsBack)
			verified.push_back(dir);
	}
	return verified;
}

static Cardinal::Value GetCheckDir(Int3 _pos, Int3 _connectionPos) {
	Int3 connectionPosModified = _connectionPos;
	connectionPosModified.y = _pos.y;
	auto d = connectionPosModified - _pos;

	// This is cancerous
	Vec3 deltaDouble = { double(d.x), double(d.y), double(d.z) };
	auto deltaDir = deltaDouble.GetFacingDirections()[0];
	return Cardinal::Opposite(deltaDir);
}

bool RailManager::CanConnect(WorldManager& _world, Int3 _pos, Cardinal::Value _dir) {
	Int3 connectionPos = FindRailConnection(_world, _pos, _dir);
	if (connectionPos == _pos)
		return false;

	auto neighborConnections = GetVerifiedConnections(_world, connectionPos);
	auto checkDir = GetCheckDir(_pos, connectionPos);

	// Is our neighbor already pointing to us?
	for (auto& dir : neighborConnections) {
		if (dir == checkDir)
			return true;
	}
	return neighborConnections.size() < 2;
}

bool RailManager::IsActuallyConnected(WorldManager& _world, Int3 _pos, Cardinal::Value _dir) {
	Int3 connectionPos = FindRailConnection(_world, _pos, _dir);
	if (connectionPos == _pos)
		return false;

	auto checkDir = GetCheckDir(_pos, connectionPos);
	for (auto& dir : GetVerifiedConnections(_world, connectionPos)) {
		if (dir == checkDir)
			return true;
	}
	return false;
}

int RailManager::GetAdjacentTrackCount(WorldManager& _world, Int3 _pos) {
	int count = 0;
	for (auto dir :
	     { Cardinal::Value::North, Cardinal::Value::South, Cardinal::Value::East, Cardinal::Value::West }) {
		if (FindRailConnection(_world, _pos, dir) != _pos)
			count++;
	}
	return count;
}

static Blocks::RailShape ResolveCleanShape(bool _cn, bool _cs, bool _ce, bool _cw, bool _canCurve) {
	Blocks::RailShape shape = Blocks::RailShape::INVALID;
	bool connectedOnZ = _cn || _cs;
	bool connectedOnX = _ce || _cw;

	if (connectedOnZ && !connectedOnX)
		shape = Blocks::RailShape::FlatNorthSouth;
	if (connectedOnX && !connectedOnZ)
		shape = Blocks::RailShape::FlatEastWest;

	if (_canCurve) {
		if (_cs && _ce && !_cn && !_cw)
			shape = Blocks::RailShape::CurveSouthEast;
		if (_cs && _cw && !_cn && !_ce)
			shape = Blocks::RailShape::CurveSouthWest;
		if (_cn && _ce && !_cs && !_cw)
			shape = Blocks::RailShape::CurveNorthEast;
		if (_cn && _cw && !_cs && !_ce)
			shape = Blocks::RailShape::CurveNorthWest;
	}
	return shape;
}

static Blocks::RailShape ApplySlope(WorldManager& _world, Int3 _pos, Blocks::RailShape _shape) {
	if (_shape == Blocks::RailShape::FlatNorthSouth) {
		if (RailManager::IsRail(
		        _world.GetBlockId(_pos.WithOffset(Cardinal::Value::North).WithOffset(Cardinal::Value::Up))))
			_shape = Blocks::RailShape::AscendingNorth;
		if (RailManager::IsRail(
		        _world.GetBlockId(_pos.WithOffset(Cardinal::Value::South).WithOffset(Cardinal::Value::Up))))
			_shape = Blocks::RailShape::AscendingSouth;
	}
	if (_shape == Blocks::RailShape::FlatEastWest) {
		if (RailManager::IsRail(
		        _world.GetBlockId(_pos.WithOffset(Cardinal::Value::East).WithOffset(Cardinal::Value::Up))))
			_shape = Blocks::RailShape::AscendingEast;
		if (RailManager::IsRail(
		        _world.GetBlockId(_pos.WithOffset(Cardinal::Value::West).WithOffset(Cardinal::Value::Up))))
			_shape = Blocks::RailShape::AscendingWest;
	}
	return _shape;
}

Blocks::RailShape RailManager::DetermineRailShape(WorldManager& _world, Int3 _pos, BlockType _block) {
	bool canCurve = RailManager::RailCanCurve(_block);

	auto cn = CanConnect(_world, _pos, Cardinal::Value::North);
	auto cs = CanConnect(_world, _pos, Cardinal::Value::South);
	auto ce = CanConnect(_world, _pos, Cardinal::Value::East);
	auto cw = CanConnect(_world, _pos, Cardinal::Value::West);

	Blocks::RailShape shape = ResolveCleanShape(cn, cs, ce, cw, canCurve);

	if (shape == Blocks::RailShape::INVALID) {
		if (cn || cs)
			shape = Blocks::RailShape::FlatNorthSouth;
		if (ce || cw)
			shape = Blocks::RailShape::FlatEastWest;

		if (canCurve) {
			// This is why powering a rail flips it!
			bool poweredHere = RedstoneManager::IsPositionPowered(_world, _pos);
			if (poweredHere) {
				if (cs && ce)
					shape = Blocks::RailShape::CurveSouthEast;
				if (cs && cw)
					shape = Blocks::RailShape::CurveSouthWest;
				if (cn && ce)
					shape = Blocks::RailShape::CurveNorthEast;
				if (cn && cw)
					shape = Blocks::RailShape::CurveNorthWest;
			} else {
				if (cn && cw)
					shape = Blocks::RailShape::CurveNorthWest;
				if (cn && ce)
					shape = Blocks::RailShape::CurveNorthEast;
				if (cs && cw)
					shape = Blocks::RailShape::CurveSouthWest;
				if (cs && ce)
					shape = Blocks::RailShape::CurveSouthEast;
			}
		}
	}

	shape = ApplySlope(_world, _pos, shape);
	return shape == Blocks::RailShape::INVALID ? Blocks::RailShape::FlatNorthSouth : shape;
}

Blocks::RailShape RailManager::DetermineRailShapeWithAddition(WorldManager& _world, Int3 _pos, BlockType _block,
                                                              Cardinal::Value _newDir) {
	bool cn = false, cs = false, ce = false, cw = false;
	auto mark = [&](Cardinal::Value _dir) {
		switch (_dir) {
		case Cardinal::Value::North:
			cn = true;
			break;
		case Cardinal::Value::South:
			cs = true;
			break;
		case Cardinal::Value::East:
			ce = true;
			break;
		case Cardinal::Value::West:
			cw = true;
			break;
		default:
			break;
		}
	};

	for (auto dir : GetVerifiedConnections(_world, _pos))
		mark(dir);
	mark(_newDir);

	bool canCurve = RailManager::RailCanCurve(_block);
	Blocks::RailShape shape = ResolveCleanShape(cn, cs, ce, cw, canCurve);

	if (shape == Blocks::RailShape::INVALID) {
		if (cn || cs)
			shape = Blocks::RailShape::FlatNorthSouth;
		if (ce || cw)
			shape = Blocks::RailShape::FlatEastWest;
	}

	shape = ApplySlope(_world, _pos, shape);
	return shape == Blocks::RailShape::INVALID ? Blocks::RailShape::FlatNorthSouth : shape;
}

bool RailManager::IsRailPowered(WorldManager& _world, Int3 _pos) {
	auto block = _world.GetBlockId(_pos);
	if (block == BLOCK_RAIL_POWERED)
		return _world.GetMetadata(_pos) & 0b1000;
	return false;
}

static bool IsRailDirectlyPowered(WorldManager& _world, Int3 _pos) {
	return RedstoneManager::IsPositionPowered(_world, _pos) ||
	       RedstoneManager::IsPositionPowered(_world, _pos.WithOffset(Cardinal::Value::Up));
}

// Axis a rail runs along; 0 = along Z, 1 = along X
enum : int {
	AXIS_Z = 0,
	AXIS_X = 1
};

static bool SearchPoweredLine(WorldManager& _world, Int3 _pos, uint8_t _meta, bool _forward, int _depth);

static bool IsFeedingRail(WorldManager& _world, Int3 _pos, bool _forward, int _depth, int _axis) {
	if (_world.GetBlockId(_pos) != BLOCK_RAIL_POWERED)
		return false;

	uint8_t meta = _world.GetMetadata(_pos);
	int shape = meta & 7;

	// Must run along the same axis as us (vanilla doesn't care whether the track actually connects)
	if (_axis == AXIS_X && (shape == 0 || shape == 4 || shape == 5))
		return false;
	if (_axis == AXIS_Z && (shape == 1 || shape == 2 || shape == 3))
		return false;

	// Only rails that are currently on can pass power along
	if ((meta & 8) == 0)
		return false;

	if (IsRailDirectlyPowered(_world, _pos))
		return true;

	// Keep walking in the SAME direction, so the search can never loop back on itself
	return SearchPoweredLine(_world, _pos, meta, _forward, _depth + 1);
}

// Step one block along our rail in one direction and look for a feeding rail.
// Reaches up to 8 rails away from the rail being updated.
static bool SearchPoweredLine(WorldManager& _world, Int3 _pos, uint8_t _meta, bool _forward, int _depth) {
	if (_depth >= 8)
		return false;

	int axis = _meta & 7;
	bool alsoCheckBelow = true; // flat rails and the low end of slopes also look one block down

	switch (_meta & 7) {
	case 0: // flat, along Z
		_pos.z += _forward ? 1 : -1;
		break;
	case 1: // flat, along X
		_pos.x += _forward ? -1 : 1;
		break;
	case 2: // rises toward +X
		if (_forward) {
			_pos.x--;
		} else {
			_pos.x++;
			_pos.y++;
			alsoCheckBelow = false;
		}
		axis = AXIS_X;
		break;
	case 3: // rises toward -X
		if (_forward) {
			_pos.x--;
			_pos.y++;
			alsoCheckBelow = false;
		} else {
			_pos.x++;
		}
		axis = AXIS_X;
		break;
	case 4: // rises toward -Z
		if (_forward) {
			_pos.z++;
		} else {
			_pos.z--;
			_pos.y++;
			alsoCheckBelow = false;
		}
		axis = AXIS_Z;
		break;
	case 5: // rises toward +Z
		if (_forward) {
			_pos.z++;
			_pos.y++;
			alsoCheckBelow = false;
		} else {
			_pos.z--;
		}
		axis = AXIS_Z;
		break;
	default:
		// Not a valid powered rail shape
		return false;
	}

	if (IsFeedingRail(_world, _pos, _forward, _depth, axis))
		return true;
	return alsoCheckBelow && IsFeedingRail(_world, _pos.WithOffset(Cardinal::Value::Down), _forward, _depth, axis);
}

void RailManager::UpdateRailPower(WorldManager& _world, Int3 _pos, BlockType _block) {
	if (_block != BLOCK_RAIL_POWERED)
		return;

	uint8_t meta = _world.GetMetadata(_pos);
	uint8_t shape = meta & 7;

	bool shouldBePowered = IsRailDirectlyPowered(_world, _pos) || SearchPoweredLine(_world, _pos, meta, true, 0) ||
	                       SearchPoweredLine(_world, _pos, meta, false, 0);
	bool currentlyPowered = (meta & 8) != 0;
	if (shouldBePowered == currentlyPowered)
		return;

	// SetMeta notifies our neighbours, which is what makes the next rail in the line re-check itself
	_world.SetMeta(_pos, shouldBePowered ? uint8_t(shape | 8) : shape);

	_world.NotifyNeighborsOfUpdate(_pos.WithOffset(Cardinal::Value::Down), BLOCK_RAIL_POWERED);
	if (shape >= 2 && shape <= 5)
		_world.NotifyNeighborsOfUpdate(_pos.WithOffset(Cardinal::Value::Up), BLOCK_RAIL_POWERED);
}

static uint8_t EncodeMeta(WorldManager& _world, Int3 _pos, BlockType _block, Blocks::RailShape _shape) {
	uint8_t oldMeta = _world.GetMetadata(_pos);
	uint8_t poweredBit = (_block != BLOCK_RAIL) ? (oldMeta & 0x8) : 0;
	return static_cast<uint8_t>(_shape) | poweredBit;
}

void RailManager::RefreshRail(WorldManager& _world, Int3 _pos, BlockType _block, bool _forceWrite) {
	uint8_t oldMeta = _world.GetMetadata(_pos);
	auto newShape = DetermineRailShape(_world, _pos, _block);
	uint8_t newMeta = EncodeMeta(_world, _pos, _block, newShape);

	if (!_forceWrite && newMeta == oldMeta)
		return;

	_world.SetMeta(_pos, newMeta);

	for (auto& dir : GetImpliedConnections(newShape)) {
		Int3 neighborPos = FindRailConnection(_world, _pos, dir);
		if (neighborPos == _pos)
			continue; // nothing actually there

		if (!CanConnect(_world, _pos, dir))
			continue;

		auto neighborBlock = _world.GetBlockId(neighborPos);
		auto neighborShape = DetermineRailShapeWithAddition(_world, neighborPos, neighborBlock,
		                                                    Cardinal::Opposite(dir));
		uint8_t neighborMeta = EncodeMeta(_world, neighborPos, neighborBlock, neighborShape);
		_world.SetMeta(neighborPos, neighborMeta);

		if (neighborBlock == BLOCK_RAIL_POWERED)
			UpdateRailPower(_world, neighborPos, neighborBlock);
	}
}