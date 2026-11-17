/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once
#include <cstdint>

enum class PacketId : uint8_t;
struct PlayerSessionMP;
class WorldManager;
class Server;

// Dispatches a single already-identified incoming packet to its handler.
namespace PacketDispatcher {
bool Dispatch(PacketId _packetId, PlayerSessionMP& _session, WorldManager& _sessionWorld, Server& _server);
} // namespace PacketDispatcher
