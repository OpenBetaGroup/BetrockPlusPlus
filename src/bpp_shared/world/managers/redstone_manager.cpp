/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "redstone_manager.h"
#include "helpers/direction_fixer.h"
#include "logger/logger.h"
#include "world.h"
#include <deque>
#include <unordered_set>
#include <vector>

static std::deque<RedstoneUpdateInfo> torchUpdates;

void RedstoneManager::PruneTorchUpdates(WorldManager& _world) {
	while (!torchUpdates.empty() && _world.elapsedTicks - torchUpdates.front().updateTime > 100)
		torchUpdates.pop_front();
}

bool RedstoneManager::CheckTorchBurnout(WorldManager& _world, Int3 _pos, bool _logUpdate) {
	if (_logUpdate)
		torchUpdates.push_back({ _pos.x, _pos.y, _pos.z, _world.elapsedTicks });

	int count = 0;
	for (auto& entry : torchUpdates) {
		if (entry.x == _pos.x && entry.y == _pos.y && entry.z == _pos.z) {
			if (++count >= 8)
				return true;
		}
	}
	return false;
}

static bool IsPoweredByAttachedLeverOrButton(WorldManager& _world, Int3 _pos) {
	static constexpr Cardinal::Value ALL_DIRS[6] = {
		Cardinal::Value::North, Cardinal::Value::South, Cardinal::Value::East,
		Cardinal::Value::West,  Cardinal::Value::Up,    Cardinal::Value::Down,
	};
	for (auto dir : ALL_DIRS) {
		Int3 neighborPos = _pos.WithOffset(dir);
		BlockType neighborBlock = _world.GetBlockId(neighborPos);
		if (neighborBlock != BLOCK_LEVER && neighborBlock != BLOCK_BUTTON_STONE)
			continue;

		uint8_t neighborMeta = _world.GetMetadata(neighborPos);
		if (!(neighborMeta & 0b1000))
			continue; // Not switched on

		// A lever/button only powers the block it is actually mounted against
		if (GetDirectionFromMeta(neighborBlock, neighborMeta) == dir)
			return true;
	}
	return false;
}

bool RedstoneManager::CanBridgeVertical(WorldManager& _world, Int3 _pos, int _dx, int _dz, int _dyOffset) {
	bool sideIsSolid = _world.IsBlockNormalCube({ _pos.x + _dx, _pos.y, _pos.z + _dz });

	if (_dyOffset > 0) {
		bool openAboveUs = !_world.IsBlockNormalCube({ _pos.x, _pos.y + 1, _pos.z });
		return sideIsSolid && openAboveUs;
	}

	return !sideIsSolid;
}

ComponentProfile RedstoneManager::GetRedstoneDustConnectivity(WorldManager& _world, Int3 _pos) {
	// Check if this redstone dust is being redirected, and if so, where its being redirected to
	ComponentProfile thisProfile;

	// Horizontal scan
	for (int dy = -1 + _pos.y; dy <= 1 + _pos.y; dy++) {
		int d[4] = { -1, 1, 0, 0 };
		for (int i = 0; i < 4; i++) {
			auto rdx = d[i];
			auto rdz = d[3 - i];
			auto dx = rdx + _pos.x;
			auto dz = rdz + _pos.z;

			BlockType neighborBlock = _world.GetBlockId({ dx, dy, dz });
			bool canConnect = (dy == _pos.y) || (CanBridgeVertical(_world, _pos, rdx, rdz, dy - _pos.y) &&
			                                     RedstoneManager::CanProvidePower(neighborBlock));

			bool continuesHere = RedstoneManager::CanProvidePower(neighborBlock);

			// Repeaters depend on facing direction
			if (dy == _pos.y &&
			    (neighborBlock == BLOCK_REDSTONE_REPEATER_ON || neighborBlock == BLOCK_REDSTONE_REPEATER_OFF)) {
				continuesHere = false;
				auto profile = RedstoneManager::GetComponentProfile(BLOCK_REDSTONE_REPEATER_ON,
				                                                    _world.GetMetadata({ dx, dy, dz }));
				if (rdx == -1) {
					if (profile.powerNX)
						continuesHere = true;
				}
				if (rdx == 1) {
					if (profile.powerX)
						continuesHere = true;
				}
				if (rdz == -1) {
					if (profile.powerNZ)
						continuesHere = true;
				}
				if (rdz == 1) {
					if (profile.powerZ)
						continuesHere = true;
				}
			}

			if (continuesHere && canConnect) {
				if (rdx == -1) {
					thisProfile.powerNX = true;
				}
				if (rdx == 1) {
					thisProfile.powerX = true;
				}
				if (rdz == -1) {
					thisProfile.powerNZ = true;
				}
				if (rdz == 1) {
					thisProfile.powerZ = true;
				}
			}
		}
	}

	return thisProfile;
}

bool RedstoneManager::DustPowersToward(WorldManager& _world, Int3 _dustPos, int _dx, int _dz) {
	if (_world.GetBlockId(_dustPos) != BLOCK_REDSTONE || _world.GetMetadata(_dustPos) == 0)
		return false;

	auto c = GetRedstoneDustConnectivity(_world, _dustPos);

	// A lone dot powers every side
	if (!c.powerNX && !c.powerX && !c.powerNZ && !c.powerZ)
		return true;

	// Otherwise the line has to run straight into the block
	if (_dx != 0) {
		bool farSide = _dx < 0 ? c.powerNX : c.powerX;
		return farSide && !c.powerNZ && !c.powerZ;
	}
	bool farSide = _dz < 0 ? c.powerNZ : c.powerZ;
	return farSide && !c.powerNX && !c.powerX;
}

PowerProfile RedstoneManager::GetBlockPowerProfile(WorldManager& _world, Int3 _pos) {
	// Only normal cubes conduct power
	if (!_world.IsBlockNormalCube(_pos))
		return {};

	bool softPowered = false;

	// Check if a switched-on lever or button is mounted against us
	if (IsPoweredByAttachedLeverOrButton(_world, _pos)) {
		return { true, true };
	}

	// Is a powered pressure plate above us?
	auto above = _world.GetBlockId(_pos.WithOffset(Cardinal::Value::Up));
	auto aboveMeta = _world.GetMetadata(_pos.WithOffset(Cardinal::Value::Up));
	if (above == BLOCK_PRESSURE_PLATE_STONE || above == BLOCK_PRESSURE_PLATE_WOOD) {
		if (aboveMeta == 1)
			return { true, true };
	}

	// Is an above detector rail powered?
	if (above == BLOCK_RAIL_DETECTOR) {
		if (aboveMeta & 8)
			return { true, true };
	}

	// Check below us
	if (_world.GetBlockId({ _pos.x, _pos.y - 1, _pos.z }) == BLOCK_REDSTONE_TORCH_ON) {
		return { true, true };
	}

	// Check above us
	if (above == BLOCK_REDSTONE && aboveMeta > 0) {
		softPowered = true;
	}

	// Check sides
	int d[4] = { -1, 1, 0, 0 };
	for (int i = 0; i < 4; i++) {
		// TODO: Direction System
		auto rdx = d[i];
		auto rdz = d[3 - i];
		auto dx = rdx + _pos.x;
		auto dz = rdz + _pos.z;

		Int3 thisPos = { dx, _pos.y, dz };

		// Redstone dust only soft powers us if it points into us
		if (DustPowersToward(_world, thisPos, rdx, rdz))
			softPowered = true;

		// This is a repeater, see if it is facing us and powered
		if (_world.GetBlockId(thisPos) == BLOCK_REDSTONE_REPEATER_ON) {
			auto grc = RedstoneManager::GetComponentProfile(BLOCK_REDSTONE_REPEATER_ON, _world.GetMetadata(thisPos));
			if (rdx == 1) {
				if (grc.powerNX)
					return { true, true };
			} else if (rdx == -1) {
				if (grc.powerX)
					return { true, true };
			} else if (rdz == 1) {
				if (grc.powerNZ)
					return { true, true };
			} else if (rdz == -1) {
				if (grc.powerZ)
					return { true, true };
			}
		}
	}

	// We were marked as soft powered but couldn't find any harder power
	if (softPowered)
		return { true, false };
	return {}; // Block isn't powered
}

static int GetDustPowerLevel(WorldManager& _world, Int3 _pos) {
	//auto thisBlock = _world.GetBlockId(_pos);
	//auto thisMeta = _world.GetMetadata(_pos);

	// Is the block under us being hard powered?
	if (RedstoneManager::GetBlockPowerProfile(_world, _pos.WithOffset(Cardinal::Value::Down)).hardPowered)
		return 15;

	// Is the block above us being hard powered?
	if (RedstoneManager::GetBlockPowerProfile(_world, _pos.WithOffset(Cardinal::Value::Up)).hardPowered)
		return 15;

	// Can the block above us power us?
	if (RedstoneManager::GetComponentProfile(_world.GetBlockId(_pos.WithOffset(Cardinal::Value::Up)),
	                                         _world.GetMetadata(_pos.WithOffset(Cardinal::Value::Up)))
	        .powerBelow)
		return 15;

	// Check the blocks to the side of us
	int best = 0;
	int d[4] = { -1, 1, 0, 0 };
	for (int i = 0; i < 4; i++) {
		auto rdx = d[i];
		auto rdz = d[3 - i];
		auto dx = rdx + _pos.x;
		auto dz = rdz + _pos.z;

		for (int dy = _pos.y - 1; dy <= _pos.y + 1; dy++) {
			Int3 dPos = { dx, dy, dz };
			auto checkId = _world.GetBlockId(dPos);
			auto checkMeta = _world.GetMetadata(dPos);
			// We only care if we are being hard powered from the same Y level
			if (dy == _pos.y && RedstoneManager::GetBlockPowerProfile(_world, dPos).hardPowered)
				return 15;

			// A torch sitting directly beside us
			if (dy == _pos.y && checkId == BLOCK_REDSTONE_TORCH_ON) {
				auto torchMeta = checkMeta;
				auto torchProfile = RedstoneManager::GetComponentProfile(BLOCK_REDSTONE_TORCH_ON, torchMeta);

				// Direction FROM the torch TOWARD us
				bool torchPowersUs = false;
				if (rdx == 1)
					torchPowersUs = torchProfile.powerNX;
				else if (rdx == -1)
					torchPowersUs = torchProfile.powerX;
				else if (rdz == 1)
					torchPowersUs = torchProfile.powerNZ;
				else if (rdz == -1)
					torchPowersUs = torchProfile.powerZ;

				if (torchPowersUs)
					return 15;
			}

			// A repeater sitting directly beside us
			if (dy == _pos.y && checkId == BLOCK_REDSTONE_REPEATER_ON) {
				auto repeaterMeta = checkMeta;
				auto repeaterProfile = RedstoneManager::GetComponentProfile(BLOCK_REDSTONE_REPEATER_ON, repeaterMeta);

				// Direction FROM the repeater TOWARD us
				bool repeaterPowersUs = false;
				if (rdx == 1)
					repeaterPowersUs = repeaterProfile.powerNX;
				else if (rdx == -1)
					repeaterPowersUs = repeaterProfile.powerX;
				else if (rdz == 1)
					repeaterPowersUs = repeaterProfile.powerNZ;
				else if (rdz == -1)
					repeaterPowersUs = repeaterProfile.powerZ;

				if (repeaterPowersUs)
					return 15;
			}

			// A lever or button or pressure plate sitting directly beside us
			if (dy == _pos.y &&
			    (checkId == BLOCK_LEVER || checkId == BLOCK_BUTTON_STONE || checkId == BLOCK_PRESSURE_PLATE_STONE ||
			     checkId == BLOCK_PRESSURE_PLATE_WOOD || checkId == BLOCK_RAIL_DETECTOR)) {
				auto neighborBlock = _world.GetBlockId(dPos);
				auto neighborMeta = _world.GetMetadata(dPos);
				auto neighborProfile = RedstoneManager::GetComponentProfile(neighborBlock, neighborMeta);

				// Direction FROM the lever/button TOWARD us
				bool poweredTowardUs = false;
				if (rdx == 1)
					poweredTowardUs = neighborProfile.powerNX;
				else if (rdx == -1)
					poweredTowardUs = neighborProfile.powerX;
				else if (rdz == 1)
					poweredTowardUs = neighborProfile.powerNZ;
				else if (rdz == -1)
					poweredTowardUs = neighborProfile.powerZ;

				if (poweredTowardUs)
					return 15;
			}

			// Check if we are a valid connection (same level, or a valid vertical bridge)
			bool canConnect = (dy == _pos.y) || RedstoneManager::CanBridgeVertical(_world, _pos, rdx, rdz, dy - _pos.y);
			if (_world.GetBlockId(dPos) == BLOCK_REDSTONE && canConnect) {
				// This is a dust so use its current power level directly
				auto neighborLevel = _world.GetMetadata(dPos);
				if (neighborLevel > best)
					best = neighborLevel;
			}
		}
	}

	return best > 0 ? best - 1 : 0;
}

bool RedstoneManager::IsPositionPowered(WorldManager& _world, Int3 _pos) {
	// Is the block under us being powered?
	if (RedstoneManager::GetBlockPowerProfile(_world, _pos.WithOffset(Cardinal::Value::Down)).powered)
		return true;

	// Is the block above us being powered?
	if (RedstoneManager::GetBlockPowerProfile(_world, _pos.WithOffset(Cardinal::Value::Up)).powered)
		return true;

	// Can the block above us power us?
	if (RedstoneManager::GetComponentProfile(_world.GetBlockId(_pos.WithOffset(Cardinal::Value::Up)),
	                                         _world.GetMetadata(_pos.WithOffset(Cardinal::Value::Up)))
	        .powerBelow)
		return true;

	// Check the blocks to the side of us
	int d[4] = { -1, 1, 0, 0 };
	for (int i = 0; i < 4; i++) {
		auto rdx = d[i];
		auto rdz = d[3 - i];
		Int3 dPos = { rdx + _pos.x, _pos.y, rdz + _pos.z };
		auto neighborBlock = _world.GetBlockId(dPos);

		if (RedstoneManager::GetBlockPowerProfile(_world, dPos).powered)
			return true;

		if (neighborBlock == BLOCK_REDSTONE_TORCH_ON || neighborBlock == BLOCK_REDSTONE_REPEATER_ON ||
		    neighborBlock == BLOCK_LEVER || neighborBlock == BLOCK_BUTTON_STONE ||
		    neighborBlock == BLOCK_PRESSURE_PLATE_STONE || neighborBlock == BLOCK_PRESSURE_PLATE_WOOD ||
		    neighborBlock == BLOCK_RAIL_DETECTOR) {
			auto neighborProfile = RedstoneManager::GetComponentProfile(neighborBlock, _world.GetMetadata(dPos));

			// Direction FROM the neighbor TOWARD us
			bool poweredTowardUs = false;
			if (rdx == 1)
				poweredTowardUs = neighborProfile.powerNX;
			else if (rdx == -1)
				poweredTowardUs = neighborProfile.powerX;
			else if (rdz == 1)
				poweredTowardUs = neighborProfile.powerNZ;
			else if (rdz == -1)
				poweredTowardUs = neighborProfile.powerZ;

			if (poweredTowardUs)
				return true;
		}

		// Dust beside us only counts if it points into us, not if it just runs past
		if (neighborBlock == BLOCK_REDSTONE && DustPowersToward(_world, dPos, rdx, rdz))
			return true;
	}

	return false;
}

static void GetNeighbors(WorldManager& _world, Int3 _pos, std::unordered_set<Int3>& _visited, std::vector<Int3>& _order,
                         bool _forceDisableProfileCheck = false) {
	auto thisBlock = _world.GetBlockId(_pos);
	bool doProfileCheck = false;

	// Only mark visited if we are redstone dust
	if (thisBlock == BLOCK_REDSTONE) {
		if (!_visited.insert(_pos).second)
			return;
		_order.push_back(_pos);
	}

	ComponentProfile thisProfile;
	if (thisBlock != BLOCK_REDSTONE && !_forceDisableProfileCheck) {
		// Make the profile getter use the powered repeater since the unpowered repeater will return false for every direction
		thisProfile = RedstoneManager::GetComponentProfile(
		    thisBlock == BLOCK_REDSTONE_REPEATER_OFF ? BLOCK_REDSTONE_REPEATER_ON : thisBlock, _world.GetMetadata(_pos));
		doProfileCheck = true;
	}

	std::vector<Int3> neighbors;
	// Horizontal scan
	for (int dy = -1 + _pos.y; dy <= 1 + _pos.y; dy++) {
		int d[4] = { -1, 1, 0, 0 };
		for (int i = 0; i < 4; i++) {
			auto rdx = d[i];
			auto rdz = d[3 - i];
			auto dx = rdx + _pos.x;
			auto dz = rdz + _pos.z;

			if (doProfileCheck) {
				if (rdx == -1) {
					if (!thisProfile.powerNX)
						continue;
				}
				if (rdx == 1) {
					if (!thisProfile.powerX)
						continue;
				}
				if (rdz == -1) {
					if (!thisProfile.powerNZ)
						continue;
				}
				if (rdz == 1) {
					if (!thisProfile.powerZ)
						continue;
				}
			}

			bool canConnect = (dy == _pos.y) || RedstoneManager::CanBridgeVertical(_world, _pos, rdx, rdz, dy - _pos.y);

			if (_world.GetBlockId({ dx, dy, dz }) == BLOCK_REDSTONE && canConnect)
				if (!_visited.count({ dx, dy, dz }))
					neighbors.push_back({ dx, dy, dz });
		}
	}

	// If we had dust around us there might be dust connecting to them too
	if (neighbors.size() != 0) {
		for (auto& neighbor : neighbors) {
			GetNeighbors(_world, neighbor, _visited, _order);
		}
	}
};

static bool ResolvePowerLevels(WorldManager& _world, const std::vector<Int3>& _positions) {
	// Returns if values actually changed this time around
	bool hasChanged = false;
	for (auto& pos : _positions) {
		auto oldLevel = _world.GetMetadata(pos);
		auto newLevel = GetDustPowerLevel(_world, pos);
		if (oldLevel != newLevel) {
			_world.SetBlock(pos, BLOCK_REDSTONE, newLevel, false, false);
			hasChanged = true;
		}
	}
	return hasChanged;
}

// Flood fill solver
// Avoids a ton of redundant updates!
static void SolveRedstoneNetwork(WorldManager& _world, Int3 _pos) {
	std::unordered_set<Int3> visited;
	std::vector<Int3> order;
	GetNeighbors(_world, _pos, visited, order, /*disable profile checks=*/true);

	std::unordered_map<Int3, uint8_t> oldLevels;
	for (auto& pos : order) {
		oldLevels.insert({ pos, _world.GetMetadata(pos) });
	}

	while (ResolvePowerLevels(_world, order))
		;

	// Each redstone wire will try and reach further out if it went from 0->powered or powered->0
	for (auto& pos : order) {
		auto oldLevel = oldLevels.find(pos)->second;
		auto newLevel = _world.GetMetadata(pos);
		if (oldLevel != newLevel && (oldLevel == 0 || newLevel == 0)) {
			// Always notify this wire tile's own direct neighbors
			_world.NotifyNeighborsOfUpdate(pos, BLOCK_REDSTONE);

			// Reach one hop further
			Int3 neighbors[6] = {
				{ pos.x - 1, pos.y, pos.z }, { pos.x + 1, pos.y, pos.z }, { pos.x, pos.y - 1, pos.z },
				{ pos.x, pos.y + 1, pos.z }, { pos.x, pos.y, pos.z - 1 }, { pos.x, pos.y, pos.z + 1 },
			};
			for (auto& npos : neighbors) {
				if (_world.GetBlockId(npos) != BLOCK_REDSTONE) {
					_world.NotifyNeighborsOfUpdate(npos, BLOCK_REDSTONE);
				}
			}
		}
	}
}

void RedstoneManager::TriggerRedstoneUpdate(WorldManager& _world, Int3 _pos, BlockType _newBlock, BlockType _oldBlock) {
	// Vanilla has some weird update quirks so we replicate that here in one pass
	// This avoids vanilla's tendency to spam block updates
	SolveRedstoneNetwork(_world, _pos);

	// DUST:
	if (_newBlock == BLOCK_REDSTONE || _oldBlock == BLOCK_REDSTONE) {
		// Vertical updates happen no matter what
		_world.NotifyNeighborsOfUpdate({ _pos.x, _pos.y - 1, _pos.z }, BLOCK_REDSTONE);
		_world.NotifyNeighborsOfUpdate({ _pos.x, _pos.y + 1, _pos.z }, BLOCK_REDSTONE);

		auto notifyWireNeighbor = [&_world](Int3 _neighborPos) -> void {
			if (_world.GetBlockId(_neighborPos) != BLOCK_REDSTONE)
				return;

			_world.NotifyNeighborsOfUpdate(_neighborPos, BLOCK_REDSTONE); // the neighbor itself
			_world.NotifyNeighborsOfUpdate({ _neighborPos.x - 1, _neighborPos.y, _neighborPos.z }, BLOCK_REDSTONE);
			_world.NotifyNeighborsOfUpdate({ _neighborPos.x + 1, _neighborPos.y, _neighborPos.z }, BLOCK_REDSTONE);
			_world.NotifyNeighborsOfUpdate({ _neighborPos.x, _neighborPos.y - 1, _neighborPos.z }, BLOCK_REDSTONE);
			_world.NotifyNeighborsOfUpdate({ _neighborPos.x, _neighborPos.y + 1, _neighborPos.z }, BLOCK_REDSTONE);
			_world.NotifyNeighborsOfUpdate({ _neighborPos.x, _neighborPos.y, _neighborPos.z - 1 }, BLOCK_REDSTONE);
			_world.NotifyNeighborsOfUpdate({ _neighborPos.x, _neighborPos.y, _neighborPos.z + 1 }, BLOCK_REDSTONE);
		};

		notifyWireNeighbor({ _pos.x - 1, _pos.y, _pos.z });
		notifyWireNeighbor({ _pos.x + 1, _pos.y, _pos.z });
		notifyWireNeighbor({ _pos.x, _pos.y, _pos.z - 1 });
		notifyWireNeighbor({ _pos.x, _pos.y, _pos.z + 1 });
	}

	// REDSTONE TORCH:
	if (_newBlock == BLOCK_REDSTONE_TORCH_ON || _oldBlock == BLOCK_REDSTONE_TORCH_ON) {
		Int3 sixNeighbors[6] = {
			{ _pos.x - 1, _pos.y, _pos.z }, { _pos.x + 1, _pos.y, _pos.z }, { _pos.x, _pos.y - 1, _pos.z },
			{ _pos.x, _pos.y + 1, _pos.z }, { _pos.x, _pos.y, _pos.z - 1 }, { _pos.x, _pos.y, _pos.z + 1 },
		};
		for (auto& n : sixNeighbors)
			_world.NotifyNeighborsOfUpdate(n, BLOCK_REDSTONE_TORCH_ON);
	}

	// REDSTONE REPEATER:
	if (_newBlock == BLOCK_REDSTONE_REPEATER_ON || _newBlock == BLOCK_REDSTONE_REPEATER_OFF) {
		Int3 sixNeighbors[6] = {
			{ _pos.x - 1, _pos.y, _pos.z }, { _pos.x + 1, _pos.y, _pos.z }, { _pos.x, _pos.y - 1, _pos.z },
			{ _pos.x, _pos.y + 1, _pos.z }, { _pos.x, _pos.y, _pos.z - 1 }, { _pos.x, _pos.y, _pos.z + 1 },
		};
		for (auto& n : sixNeighbors)
			_world.NotifyNeighborsOfUpdate(n, _newBlock);
	}
}

void RedstoneManager::RefreshWireAt(WorldManager& _world, Int3 _pos) {
	// Redstone wires on neighbor change gets called independently of whether the update was caused by a redstone component
	SolveRedstoneNetwork(_world, _pos);
}

bool RedstoneManager::GetProfileInDirection(const ComponentProfile _profile, const Cardinal::Value _dir) {
	switch (_dir) {
	case Cardinal::Value::North:
		return _profile.powerNZ;
	case Cardinal::Value::South:
		return _profile.powerZ;
	case Cardinal::Value::West:
		return _profile.powerNX;
	case Cardinal::Value::East:
		return _profile.powerX;
	case Cardinal::Value::Down:
		return _profile.powerBelow;
	default:
		return false;
	}
}

// TODO:
bool IsValidPoweringComponent(BlockType _block) {
	return _block == BLOCK_REDSTONE_TORCH_ON || _block == BLOCK_REDSTONE_REPEATER_ON || _block == BLOCK_LEVER ||
	       _block == BLOCK_BUTTON_STONE || _block == BLOCK_PRESSURE_PLATE_STONE ||
	       _block == BLOCK_PRESSURE_PLATE_WOOD || _block == BLOCK_RAIL_DETECTOR;
}

bool RedstoneManager::IsRepeaterInputPowered(WorldManager& _world, Int3 _pos, uint8_t _meta) {
	const auto dir = GetDirectionFromMeta(BLOCK_REDSTONE_REPEATER_OFF, _meta & 3);
	Int3 inputPos = _pos.WithOffset(Cardinal::Opposite(dir));
	auto block = _world.GetBlockId(inputPos);
	auto meta = _world.GetMetadata(inputPos);
	if (IsValidPoweringComponent(block) && GetProfileInDirection(RedstoneManager::GetComponentProfile(block, meta), dir))
		return true;
	if (RedstoneManager::GetBlockPowerProfile(_world, inputPos).powered)
		return true;
	return block == BLOCK_REDSTONE && meta > 0;
}