/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once
#include "tile_entity.h"
#include <memory>
#include <vector>

// Simple wrapper so we don't have to manually add
class WorldManager;
struct TileEntityManager {
	std::vector<std::weak_ptr<TileEntity>> tickableTileEntities;

	// Initialize a tile entity into the world
	void InitializeTileEntity(const std::shared_ptr<TileEntity>& _tileEntity) {
		if (_tileEntity->canTick) {
			tickableTileEntities.push_back(_tileEntity);
		}
	}

	void TickTileEntities(WorldManager& _world);
};

namespace TileEntityHelper {
std::shared_ptr<TileEntity> CreateTileEntityShared(const Tag& _nbt);
} // namespace TileEntityHelper