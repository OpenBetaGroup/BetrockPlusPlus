/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#include "packet_dispatcher.h"

#include "../server.h"
#include "logger/logger.h"
#include <iomanip>

namespace {
template <typename Pkt, typename Fn>
void Handle(PlayerSession& _session, Fn&& _fn) {
	Pkt pkt;
	pkt.Deserialize(_session.stream);
	if (!_session.stream.IsShortRead())
		_fn(pkt);
}
} // namespace

bool PacketDispatcher::Dispatch(PacketId _packetId, PlayerSession& _session, WorldManager& _sessionWorld,
                                Server& _server) {
	if (_session.connState != ConnectionState::Playing || !_session.stream.IsConnected())
		return true;

	auto& tracker = _session.dimension == Dimension::Overworld ? _server.overworldEntityTracker
	                                                            : _server.hellEntityTracker;

	switch (_packetId) {
	case PacketId::KeepAlive:
		Handle<Packet::KeepAlive>(_session, [&](auto& pkt) { HandlePacket::KeepAlive(pkt, _session); });
		break;
	case PacketId::ChatMessage:
		Handle<Packet::ChatMessage>(_session, [&](auto& pkt) {
			HandlePacket::ChatMessage(
			    pkt, _session, _server.players, _sessionWorld, _server.commandManager, [](PlayerSession&) {},
			    _server);
		});
		break;
	case PacketId::SetTime: {
		Packet::SetTime pkt;
		pkt.Deserialize(_session.stream);
		break;
	}
	case PacketId::InteractWithEntity:
		Handle<Packet::InteractWithEntity>(
		    _session, [&](auto& pkt) { HandlePacket::InteractWithEntity(pkt, _session, _sessionWorld); });
		break;
	case PacketId::Respawn:
		Handle<Packet::Respawn>(_session, [&](auto& pkt) { HandlePacket::Respawn(pkt, _session, _server); });
		break;
	case PacketId::PlayerMovement:
		Handle<Packet::PlayerMovement>(_session, [&](auto& pkt) { HandlePacket::PlayerMovement(pkt, _session); });
		break;
	case PacketId::PlayerPosition:
		Handle<Packet::PlayerPosition>(_session,
		                               [&](auto& pkt) { HandlePacket::PlayerPosition(pkt, _session, _server); });
		break;
	case PacketId::PlayerRotation:
		Handle<Packet::PlayerRotation>(_session, [&](auto& pkt) { HandlePacket::PlayerRotation(pkt, _session); });
		break;
	case PacketId::PlayerPositionAndRotation:
		Handle<Packet::PlayerPositionAndRotation>(
		    _session, [&](auto& pkt) { HandlePacket::PlayerPositionAndRotation(pkt, _session, _server); });
		break;
	case PacketId::MineBlock:
		Handle<Packet::MineBlock>(
		    _session, [&](auto& pkt) { HandlePacket::MineBlock(pkt, _session, _sessionWorld, _server); });
		break;
	case PacketId::PlaceBlock:
		Handle<Packet::PlaceBlock>(_session, [&](auto& pkt) {
			HandlePacket::PlaceBlock(pkt, _session, _sessionWorld, _server.gameRuntime, _server);
		});
		break;
	case PacketId::SetHotbarSlot:
		Handle<Packet::SetHotbarSlot>(_session, [&](auto& pkt) { HandlePacket::SetHotbarSlot(pkt, _session); });
		break;
	case PacketId::InteractWithBlock:
		Handle<Packet::InteractWithBlock>(
		    _session, [&](auto& pkt) { HandlePacket::InteractWithBlock(pkt, _session, _sessionWorld); });
		break;
	case PacketId::Animation:
		Handle<Packet::Animation>(_session, [&](auto& pkt) { HandlePacket::Animation(pkt, _session, tracker); });
		break;
	case PacketId::PlayerAction:
		Handle<Packet::PlayerAction>(_session,
		                             [&](auto& pkt) { HandlePacket::PlayerAction(pkt, _session, tracker); });
		break;
	case PacketId::PlayerInput: { // Consumed but unused
		Packet::PlayerInput pkt;
		pkt.Deserialize(_session.stream);
		break;
	}
	case PacketId::CloseContainer:
		Handle<Packet::CloseContainer>(_session, [&](auto& pkt) { HandlePacket::CloseContainer(pkt, _session); });
		break;
	case PacketId::ClickSlot:
		Handle<Packet::ClickSlot>(_session, [&](auto& pkt) { HandlePacket::ClickSlot(pkt, _session); });
		break;
	case PacketId::ContainerTransaction:
		Handle<Packet::ContainerTransaction>(
		    _session, [&](auto& pkt) { HandlePacket::ContainerTransaction(pkt, _session); });
		break;
	case PacketId::UpdateSign:
		Handle<Packet::UpdateSign>(_session, [&](auto& pkt) {
			HandlePacket::UpdateSign(pkt, _session, _sessionWorld, _server.players);
		});
		break;
	case PacketId::Disconnect: {
		Packet::Disconnect pkt;
		pkt.Deserialize(_session.stream);
		if (_session.stream.IsShortRead())
			return true;
		HandlePacket::Disconnect(pkt, _session, _server);
		return false; // session is dead; stop processing
	}
	default:
		Logger::Warning << "UNHANDLED packet 0x" << std::hex << static_cast<int>(_packetId) << std::dec << "\n";
		_server.connStateManager.DisconnectPlayer(_session, "Unknown packet", _server);
		return false;
	}
	return true;
}
