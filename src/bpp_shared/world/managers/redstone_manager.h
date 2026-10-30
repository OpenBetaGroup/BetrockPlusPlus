/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once
#include "../helpers/direction_fixer.h"
#include "base_structs.h"
#include "blocks.h"
#include "blocks/block_behaviors.h"
#include "blocks/block_properties_behaviors.h"

// For redstone!
class WorldManager;
struct ComponentProfile {
	// These are for purely whether we can power other components / dust
	// NOT if we can power that block!
	bool powerX : 1 = false;
	bool powerNX : 1 = false;
	bool powerZ : 1 = false;
	bool powerNZ : 1 = false;
	bool powerBelow : 1 = false;
};

struct PowerProfile {
	bool powered : 1 = false;
	bool hardPowered : 1 = false;
};

struct RedstoneUpdateInfo {
	int32_t x, y, z;
	TickTime updateTime;
};

namespace RedstoneManager {
void TriggerRedstoneUpdate(WorldManager& _world, Int3 _pos, BlockType _newBlock, BlockType _oldBlock);

// Call this from redstone dust's onNeighborBlockChange
void RefreshWireAt(WorldManager& _world, Int3 _pos);
bool CanBridgeVertical(WorldManager& _world, Int3 _pos, int _dx, int _dz, int _dyOffset);
bool IsRepeaterInputPowered(WorldManager& _world, Int3 _pos, uint8_t _meta);
ComponentProfile GetRedstoneDustConnectivity(WorldManager& _world, Int3 _pos);
// Does powered dust at _dustPos power the block beside it? (_dx, _dz) is the offset FROM that block TO the dust.
bool DustPowersToward(WorldManager& _world, Int3 _dustPos, int _dx, int _dz);
PowerProfile GetBlockPowerProfile(WorldManager& _world, Int3 _pos);
bool IsPositionPowered(WorldManager& _world, Int3 _pos);
void PruneTorchUpdates(WorldManager& _world);
bool CheckTorchBurnout(WorldManager& _world, Int3 _pos, bool _logUpdate);
bool GetProfileInDirection(const ComponentProfile _profile, const Cardinal::Value _dir);

inline bool CanProvidePower(BlockType _block) {
	// Repeaters are excluded for some reason in vanilla
	switch (_block) {
	case BLOCK_REDSTONE:
	case BLOCK_LEVER:
	case BLOCK_BUTTON_STONE:
	case BLOCK_PRESSURE_PLATE_STONE:
	case BLOCK_PRESSURE_PLATE_WOOD:
	case BLOCK_REDSTONE_TORCH_ON:
	case BLOCK_REDSTONE_TORCH_OFF:
	case BLOCK_RAIL_DETECTOR:
		return true;
	default:
		return false;
	}
}

inline bool CanTriggerRedstoneUpdate(BlockType _block) {
	switch (_block) {
	case BLOCK_REDSTONE:
	case BLOCK_LEVER:
	case BLOCK_BUTTON_STONE:
	case BLOCK_PRESSURE_PLATE_STONE:
	case BLOCK_PRESSURE_PLATE_WOOD:
	case BLOCK_REDSTONE_TORCH_ON:
	case BLOCK_REDSTONE_TORCH_OFF:
	case BLOCK_REDSTONE_REPEATER_ON:
	case BLOCK_REDSTONE_REPEATER_OFF:
	case BLOCK_RAIL_DETECTOR:
		return true;
	default:
		return false;
	}
}

inline ComponentProfile GetComponentProfile(BlockType _blockId, uint8_t _meta) {
	switch (_blockId) {
	case BLOCK_REDSTONE: {
		return ComponentProfile{ true, true, true, true, false };
	}
	case BLOCK_RAIL_DETECTOR: {
		if (_meta & 8)
			return ComponentProfile{ true, true, true, true, false };
		return {};
	}
	case BLOCK_REDSTONE_TORCH_ON: {
		switch (GetDirectionFromMeta(BLOCK_REDSTONE_TORCH_ON, _meta)) {
		case Cardinal::Value::North:
			return ComponentProfile{ true, true, false, true, true };
		case Cardinal::Value::South:
			return ComponentProfile{ true, true, true, false, true };
		case Cardinal::Value::East:
			return ComponentProfile{ true, false, true, true, true };
		case Cardinal::Value::West:
			return ComponentProfile{ false, true, true, true, true };
		case Cardinal::Value::Up:
			return ComponentProfile{ true, true, true, true, false };
		default:
			return {};
		}
	}
	case BLOCK_REDSTONE_TORCH_OFF: {
		return {};
	}
	case BLOCK_PRESSURE_PLATE_STONE:
	case BLOCK_PRESSURE_PLATE_WOOD: {
		if (_meta == 1) {
			return ComponentProfile{ true, true, true, true, false };
		}
		return {};
	}
	case BLOCK_REDSTONE_REPEATER_ON: {
		switch (GetDirectionFromMeta(BLOCK_REDSTONE_REPEATER_ON, _meta)) {
		case Cardinal::Value::North:
			return ComponentProfile{ false, false, false, true, false }; // outputs -Z
		case Cardinal::Value::East:
			return ComponentProfile{ true, false, false, false, false }; // outputs +X
		case Cardinal::Value::South:
			return ComponentProfile{ false, false, true, false, false }; // outputs +Z
		case Cardinal::Value::West:
			return ComponentProfile{ false, true, false, false, false }; // outputs -X
		default:
			return {};
		}
	}
	case BLOCK_REDSTONE_REPEATER_OFF: {
		return {};
	}
	case BLOCK_LEVER: {
		if (!(_meta & 0b1000))
			return {};
		switch (GetDirectionFromMeta(BLOCK_LEVER, _meta)) {
		case Cardinal::Value::North:
			return ComponentProfile{ true, true, false, true, true };
		case Cardinal::Value::South:
			return ComponentProfile{ true, true, true, false, true };
		case Cardinal::Value::East:
			return ComponentProfile{ true, false, true, true, true };
		case Cardinal::Value::West:
			return ComponentProfile{ false, true, true, true, true };
		case Cardinal::Value::Up:
			return ComponentProfile{ true, true, true, true, false };
		default:
			return {};
		}
	}
	case BLOCK_BUTTON_STONE: {
		if (!(_meta & 0b1000))
			return {};
		switch (GetDirectionFromMeta(BLOCK_BUTTON_STONE, _meta)) {
		case Cardinal::Value::North:
			return ComponentProfile{ true, true, false, true, true };
		case Cardinal::Value::South:
			return ComponentProfile{ true, true, true, false, true };
		case Cardinal::Value::East:
			return ComponentProfile{ true, false, true, true, true };
		case Cardinal::Value::West:
			return ComponentProfile{ false, true, true, true, true };
		case Cardinal::Value::Up:
			return ComponentProfile{ true, true, true, true, false };
		default:
			return {};
		}
	}
	default: {
		return {};
	}
	}
}
}; // namespace RedstoneManager