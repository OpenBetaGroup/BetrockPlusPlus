/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#include "world_event_broadcaster.h"

#include "../server.h"
void WorldEventBroadcaster::BroadcastNoteEvent(Server& _server, Int3 _position, int _instrumentState,
                                               int _instrumentDirection, Dimension _dimension, double _rangeSq) {
	Packet::BlockEvent pkt;
	pkt.position = { _position.x, int16_t(_position.y), _position.z };
	pkt.instrumentState = _instrumentState;
	pkt.pitchDirection = _instrumentDirection;

	// Only players that are actually connected and in the right dimension can hear/see this
	std::vector<PlayerSessionMP*> inRange;
	for (auto& session : _server.GetPlayers()) {
		if (session->connState != ConnectionState::Playing)
			continue;
		if (session->dimension != _dimension)
			continue;

		Vec3 playerPos = session->position;
		double dx = playerPos.x - (double(_position.x) + 0.5);
		double dy = playerPos.y - (double(_position.y) + 0.5);
		double dz = playerPos.z - (double(_position.z) + 0.5);
		double distSq = dx * dx + dy * dy + dz * dz;
		if (distSq > _rangeSq)
			continue;

		inRange.push_back(session.get());
	}

	if (inRange.empty())
		return;

	NetworkStream tmpStream(-1);
	pkt.Serialize(tmpStream);
	const auto& buf = tmpStream.GetRawWriteBuffer();
	for (auto* session : inRange)
		session->stream.WriteRaw(buf.data(), buf.size());
}

void WorldEventBroadcaster::BroadcastExplosion(Server& _server, Vec3 _position, float _size,
                                               const std::unordered_set<Int3>& _blocks, Dimension _dimension,
                                               double _rangeSq) {
	Packet::Explosion pkt;
	pkt.position = _position;
	pkt.radius = _size;
	pkt.numberOfDestroyedBlocks = int32_t(_blocks.size());

	// Offsets are relative to the truncated explosion position, like Java's (int) cast
	Int3 origin = { int(_position.x), int(_position.y), int(_position.z) };
	pkt.destroyedBlocks.reserve(_blocks.size() * 3);
	for (const auto& pos : _blocks) {
		pkt.destroyedBlocks.push_back(static_cast<int8_t>(pos.x - origin.x));
		pkt.destroyedBlocks.push_back(static_cast<int8_t>(pos.y - origin.y));
		pkt.destroyedBlocks.push_back(static_cast<int8_t>(pos.z - origin.z));
	}

	std::vector<PlayerSessionMP*> inRange;
	for (auto& session : _server.GetPlayers()) {
		if (session->connState != ConnectionState::Playing)
			continue;
		if (session->dimension != _dimension)
			continue;

		Vec3 playerPos = session->position;
		double dx = _position.x - playerPos.x;
		double dy = _position.y - playerPos.y;
		double dz = _position.z - playerPos.z;
		// Strictly less than, same as ServerConfigurationManager.sendPacketToPlayersAroundPoint
		if (dx * dx + dy * dy + dz * dz >= _rangeSq)
			continue;

		inRange.push_back(session.get());
	}

	if (inRange.empty())
		return;

	NetworkStream tmpStream(-1);
	pkt.Serialize(tmpStream);
	const auto& buf = tmpStream.GetRawWriteBuffer();
	for (auto* session : inRange)
		session->stream.WriteRaw(buf.data(), buf.size());
}

void WorldEventBroadcaster::BroadcastWorldEvent(Server& _server, PacketData::WorldEvent _eventType, Int3 _position,
                                                int32_t _data, Dimension _dimension,
                                                PlayerSessionMP* _triggeringSession,
                                                double _rangeSq) {
	Packet::WorldEvent pkt;
	pkt.eventType = _eventType;
	pkt.position = { _position.x, static_cast<int8_t>(_position.y), _position.z };
	pkt.data = _data;

	// Only players that are actually connected and in the right dimension can hear/see this,
	// same rule the wiki documents for sounds (e.g. music discs).
	std::vector<PlayerSessionMP*> inRange;
	for (auto& session : _server.players) {
		if (session.get() == _triggeringSession)
			continue;
		if (session->connState != ConnectionState::Playing)
			continue;
		if (session->dimension != _dimension)
			continue;

		Vec3 playerPos = session->position;
		double dx = playerPos.x - (double(_position.x) + 0.5);
		double dy = playerPos.y - (double(_position.y) + 0.5);
		double dz = playerPos.z - (double(_position.z) + 0.5);
		double distSq = dx * dx + dy * dy + dz * dz;
		if (distSq > _rangeSq)
			continue;

		inRange.push_back(session.get());
	}

	if (inRange.empty())
		return;

	NetworkStream tmpStream(-1);
	pkt.Serialize(tmpStream);
	const auto& buf = tmpStream.GetRawWriteBuffer();
	for (auto* session : inRange)
		session->stream.WriteRaw(buf.data(), buf.size());
}