/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "world_wrapper.h"

// Grab the 3x3 region. Any chunk that is already inUse is left as nullptr
// (writes to it will fall through to the deferred path via the manager).
void WorldWrapper::GetChunkRegion() {
	for (int dx = -1; dx <= 1; dx++) {
		for (int dz = -1; dz <= 1; dz++) {
			int ax = centerChunkPos.x + dx;
			int az = centerChunkPos.z + dz;
			auto c = manager.GetChunk({ ax, az });
			if (!c || c->inUse.load()) {
				chunkRegion.chunks[dx + 1][dz + 1] = nullptr;
			} else {
				chunkRegion.chunks[dx + 1][dz + 1] = c;
			}
		}
	}
	// Mark all successfully acquired chunks as inUse
	for (auto& row : chunkRegion.chunks)
		for (auto& c : row)
			if (c)
				c->inUse.store(true);
}

void WorldWrapper::FreeChunkRegion() {
	for (auto& row : chunkRegion.chunks)
		for (auto& c : row)
			if (c)
				c->inUse.store(false);
}

// Convert a world-space position to a region-local chunk offset (-1..1, -1..1)
Int2 WorldWrapper::GetRegionChunkPos(const Int3 _wPos) const {
	return { (_wPos.x >> 4) - centerChunkPos.x, (_wPos.z >> 4) - centerChunkPos.z };
}

int WorldWrapper::FindTopSolidBlock(const int _wx, const int _wz) {
	auto chunk = chunkRegion.GetChunk(GetRegionChunkPos({ _wx, 0, _wz }));
	if (!chunk || chunk->state.load() < ChunkState::Generated)
		return -1;
	int lx = _wx & 15, lz = _wz & 15;
	for (int y = CHUNK_HEIGHT - 1; y > 0; --y) {
		BlockType block = chunk->GetBlock({ lx, y, lz });
		if (block == BlockType::BLOCK_AIR)
			continue;
		Material mat = Blocks::blockProperties[block].material;
		if (mat.isSolid || mat.isLiquid)
			return y + 1;
	}
	return -1;
}

int WorldWrapper::GetHeightValue(const int _wx, const int _wz) {
	auto chunk = chunkRegion.GetChunk(GetRegionChunkPos({ _wx, 0, _wz }));
	if (!chunk || chunk->state.load() < ChunkState::Generated)
		return 0;
	return chunk->GetHeightValue({ _wx & 15, _wz & 15 });
}

double WorldWrapper::GetTemperatureAt(const int _wx, const int _wz) {
	auto chunk = chunkRegion.GetChunk(GetRegionChunkPos({ _wx, 0, _wz }));
	if (!chunk || chunk->state.load() < ChunkState::Generated)
		return 0.5;
	manager.EnsureClimate(*chunk);
	return double(chunk->GetTemperature({ _wx & 15, _wz & 15 }));
}

double WorldWrapper::GetHumidityAt(const int _wx, const int _wz) {
	auto chunk = chunkRegion.GetChunk(GetRegionChunkPos({ _wx, 0, _wz }));
	if (!chunk || chunk->state.load() < ChunkState::Generated)
		return 0.5;
	manager.EnsureClimate(*chunk);
	return double(chunk->GetHumidity({ _wx & 15, _wz & 15 }));
}

BlockType WorldWrapper::GetBlockId(const Int3 _wpos) {
	if (!InBounds(_wpos.y))
		return BlockType::BLOCK_AIR;
	auto chunk = chunkRegion.GetChunk(GetRegionChunkPos(_wpos));
	// Falls outside our grabbed region -> ask the manager directly (read-only, safe)
	if (!chunk)
		return manager.GetBlockId(_wpos);
	if (chunk->state.load() < ChunkState::Generated)
		return BlockType::BLOCK_AIR;
	return chunk->GetBlock({ _wpos.x & 15, _wpos.y, _wpos.z & 15 });
}

void WorldWrapper::SetBlock(const Int3 _wpos, const BlockType _type, const uint8_t _meta,
                            const bool /*_keepTileEntity*/, const bool /*_updateNeighbors*/) {
	if (!InBounds(_wpos.y))
		return;
	auto chunk = chunkRegion.GetChunk(GetRegionChunkPos(_wpos));
	if (!chunk || chunk->state.load() < ChunkState::Generated) {
		// Outside our locked region
		manager.SetBlock(_wpos, _type, _meta);
		return;
	}

	// Remove any tile entities that exist at this spot
	auto& tes = chunk->tileEntities;
	if (!tes.empty()) {
		tes.erase(std::remove_if(tes.begin(), tes.end(),
		                         [&](const std::shared_ptr<TileEntity>& _te) { return _te && _te->position == _wpos; }),
		          tes.end());
	}

	// Get the local coordinates of this block within the chunk and set it
	const Int2 localXz{ _wpos.x & 15, _wpos.z & 15 };
	const Int3 local{ localXz.x, _wpos.y, localXz.z };
	const auto oldBlock = chunk->GetBlock(local);
	//auto oldMeta = chunk->GetMeta(local);

	// Making the assumption here that certain metadatas of
	// blocks don't have differing light properties
	const bool changesLighting = (Blocks::blockProperties[_type].lightOpacity !=
	                              Blocks::blockProperties[oldBlock].lightOpacity) ||
	                             (Blocks::blockProperties[_type].lightEmission !=
	                              Blocks::blockProperties[oldBlock].lightEmission);

	// Unlight before changing the block
	if (changesLighting) {
		manager.lightManager.UnlightAt(_wpos.x, _wpos.y, _wpos.z, LightType::Block, manager);
		manager.lightManager.UnlightAt(_wpos.x, _wpos.y, _wpos.z, LightType::Sky, manager);
	}

	// Then finally set the new block
	chunk->SetBlockRaw(local, _type);
	chunk->SetMetaRaw(local, _meta);
	chunk->isModified = true;

	const int oldHeight = chunk->GetHeightValue(localXz);

	// Placing opaque block; heightmap may rise
	if (changesLighting) {
		chunk->RelightColumn(localXz);
		const int newHeight = chunk->GetHeightValue(localXz);
		if (newHeight > oldHeight) {
			// Notify the BFS that all blocks from y down to oldHeight need updating
			for (int sy = oldHeight; sy <= newHeight; ++sy) {
				manager.lightManager.UnlightAt(_wpos.x, sy, _wpos.z, LightType::Sky, manager);
			}
		} else if (newHeight < oldHeight) {
			// Height fell
			for (int sy = newHeight; sy < oldHeight; ++sy) {
				manager.lightManager.ScheduleLightUpdate({ _wpos.x, sy, _wpos.z }, LightType::Sky);
			}
		}

		// Always re-evaluate the edited block and its 4 horizontal neighbours
		manager.lightManager.ScheduleLightUpdate(_wpos, LightType::Sky);
		manager.lightManager.ScheduleLightUpdate(_wpos, LightType::Block);
		int extendedBottom = std::min<int>(newHeight, oldHeight);
		while (extendedBottom > 0 &&
		       Blocks::blockProperties[chunk->GetBlock({ local.x, extendedBottom - 1, local.z })].lightOpacity == 0)
			--extendedBottom;

		if (newHeight != oldHeight) {
			manager.lightManager.ScheduleLightRegion(
			    { _wpos.x - 1, extendedBottom, _wpos.z - 1 },
			    { _wpos.x + 1, std::max<int>(newHeight, oldHeight), _wpos.z + 1 }, LightType::Sky);
		}
	}

	// So water and lava flow
	if (_type == BLOCK_WATER_FLOWING || _type == BLOCK_LAVA_FLOWING) {
		auto function = Blocks::blockBehaviors[_type].onBlockAdded;
		if (function)
			function(manager, _wpos);
	}

	// Callback for the client and server to know about this block update
	if (manager.onBlockUpdate)
		manager.onBlockUpdate(PendingBlock{ .block{ _type, _meta },
		                                    .blockPos = _wpos,
		                                    .light{ chunk->GetBlockLight(local), chunk->GetSkyLight(local) } },
		                      chunk->cpos);
}

uint8_t WorldWrapper::GetSkyLight(const Int3 _wpos) {
	if (!InBounds(_wpos.y))
		return 0;
	auto chunk = chunkRegion.GetChunk(GetRegionChunkPos(_wpos));
	if (!chunk || chunk->state.load() < ChunkState::Generated)
		return 0;
	return chunk->GetSkyLight({ _wpos.x & 15, _wpos.y, _wpos.z & 15 });
}

int WorldWrapper::GetBlockLightRaw(const Int3 _wpos) {
	if (!InBounds(_wpos.y))
		return 15;
	auto chunk = chunkRegion.GetChunk(GetRegionChunkPos(_wpos));
	if (!chunk || chunk->state.load() < ChunkState::Generated)
		return 15;
	Int3 localPos = { _wpos.x & 15, _wpos.y, _wpos.z & 15 };
	int skylight = chunk->GetSkyLight(localPos);
	int blockLight = chunk->GetBlockLight(localPos);
	return blockLight > skylight ? blockLight : skylight;
}