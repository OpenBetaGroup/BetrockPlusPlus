/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once
#include "dimensions.h"
#include "numeric_structs.h"
#include "world/world.h"
#include <unordered_map>
#include <vector>

class Server;

// Sends accumulated per-Tick block changes out to whichever player sessions care about them
namespace ChunkBroadcaster {
void BroadcastBlockChanges(Server& _server, std::unordered_map<Int32_2, std::vector<PendingBlock>>& _changes,
                           Dimension _dimension, WorldManager& _dimWorld);
} // namespace ChunkBroadcaster
