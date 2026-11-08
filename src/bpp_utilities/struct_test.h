/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once
#include "chunk.h"
#include "entities/entity.h"
#include "logger.h"
#include "player_conn/player_session.h"
#include "storage/region.h"
#include "storage/region_manager.h"
#include "world/generator/nether/chunk_gen.h"
#include "world/generator/overworld/chunk_gen.h"

void PrintStructSizes() {
	Logger::Debug << "Chunk: " << sizeof(Chunk) << " Bytes (includes " << SUB_CHUNK_COUNT << " x " << sizeof(SubChunk)
	              << " Byte sub-chunk headers)\n";
	Logger::Debug << "SubChunk layers, allocated independently: Blocks (types + counts) "
	              << sizeof(SubChunk::BlockLayer) << " Bytes, Meta/BlockLight/SkyLight "
	              << sizeof(SubChunk::NibbleLayer) << " Bytes each\n";
	Logger::Debug << "RegionManager: " << sizeof(RegionManager) << " Bytes\n";
	Logger::Debug << "Region: " << sizeof(Region) << " Bytes\n";
	Logger::Debug << "Entity: " << sizeof(Entity) << " Bytes\n";
	Logger::Debug << "PlayerSession: " << sizeof(PlayerSession) << " Bytes\n";
	Logger::Debug << "OverworldGenerator: " << sizeof(OverworldGenerator) << " Bytes\n";
	Logger::Debug << "NetherGenerator: " << sizeof(NetherGenerator) << " Bytes\n";
}