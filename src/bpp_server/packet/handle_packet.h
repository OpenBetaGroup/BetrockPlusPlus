/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once
#include <functional>
#include "platform/platform_caps.h"
#if defined(BPP_HAS_POSIX_SOCKETS)
#define INVALID_SOCKET -1
#include <netinet/in.h>
#ifndef BPP_PLATFORM_CONSOLE
#include <sys/ioctl.h>
#endif
#include <sys/socket.h>
#include <unistd.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include "../commands/command_manager.h"
#include "../player_conn/player_session.h"
#include "blocks/block_properties.h"
#include "networking/packets.h"
#include "runtime.h"
#include "world/world.h"

struct EntityTracker;
// _margin pads the protected radius (for placements that can spill past the target block)
bool IsValidActionInSpawnRadius(PlayerSession& _session, Server& _server, Int3 _pos, WorldManager& _world,
                                int _margin = 0);
namespace HandlePacket {
void KeepAlive(Packet::KeepAlive& /*pkt*/, PlayerSession& _session);
void ChatMessage(Packet::ChatMessage& _pkt, PlayerSession& _session,
                 std::vector<std::shared_ptr<PlayerSession>>& _players, WorldManager& _world, CommandManager& _cmdMgr,
                 std::function<void(PlayerSession&)> _transferDimension, Server& _server);
void PlayerMovement(Packet::PlayerMovement& /*pkt*/, PlayerSession& /*session*/);
void PlayerPosition(Packet::PlayerPosition& _pkt, PlayerSession& _session, Server& _server);
void PlayerRotation(Packet::PlayerRotation& _pkt, PlayerSession& _session);
void PlayerPositionAndRotation(Packet::PlayerPositionAndRotation& _pkt, PlayerSession& _session, Server& _server);
void MineBlock(Packet::MineBlock& _pkt, PlayerSession& _session, WorldManager& _world, Server& _server);
void PlaceBlock(Packet::PlaceBlock& _pkt, PlayerSession& _session, WorldManager& _world, Runtime& _gameRuntime,
                Server& _server);
void SetHotbarSlot(Packet::SetHotbarSlot& _pkt, PlayerSession& _session);
// Click handler
void ClickSlot(Packet::ClickSlot& _pkt, PlayerSession& _session);
void CloseContainer(Packet::CloseContainer& /*pkt*/, PlayerSession& _session);
// Client acknowledges a rejected transaction
void ContainerTransaction(Packet::ContainerTransaction& _pkt, PlayerSession& _session);
// Other handlers
void InteractWithEntity(Packet::InteractWithEntity& _pkt, PlayerSession& _session, WorldManager& _world);
void InteractWithBlock(Packet::InteractWithBlock& _pkt, PlayerSession& _session, WorldManager& _world);
void Animation(Packet::Animation& _pkt, PlayerSession& _session, EntityTracker& _entityTracker);
void PlayerAction(Packet::PlayerAction& _pkt, PlayerSession& _session, EntityTracker& _entityTracker);
void Respawn(Packet::Respawn& _pkt, PlayerSession& _session, Server& _server);
void UpdateSign(Packet::UpdateSign& _pkt, PlayerSession& _session, WorldManager& _world,
                std::vector<std::shared_ptr<PlayerSession>>& _players);
void Disconnect(Packet::Disconnect& _pkt, PlayerSession& _session, Server& _server);
} // namespace HandlePacket