/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#include "chunk_broadcaster.h"

#include "../server.h"

void ChunkBroadcaster::BroadcastBlockChanges(Server& _server,
                                             std::unordered_map<Int32_2, std::vector<PendingBlock>>& _changes,
                                             Dimension _dimension, WorldManager& _dimWorld) {
	for (auto& [chunk, blockChanges] : _changes) {
		{
			std::vector<PendingBlock> unique;
			unique.reserve(blockChanges.size());
			std::unordered_map<int, size_t> indexOf;
			for (const auto& pb : blockChanges) {
				int key = (pb.blockPos.x & 15) | ((pb.blockPos.z & 15) << 4) | (pb.blockPos.y << 8);
				auto [it, inserted] = indexOf.try_emplace(key, unique.size());
				if (inserted)
					unique.push_back(pb);
				else
					unique[it->second] = pb;
			}
			blockChanges.swap(unique);
		}

		// Find which sessions care about this chunk
		// Split into flushed (send immediately) and sentOnly (queue).
		auto indexIt = _server.chunkSessions.find(Server::ChunkKey(chunk, _dimension));
		std::vector<PlayerSession*> flushedSessions;
		std::vector<PlayerSession*> sentOnlySessions;

		if (indexIt != _server.chunkSessions.end())
			flushedSessions = indexIt->second;

		// Sessions that have the chunk in-flight (sentChunks but not flushedChunks) still need to queue the updates.
		for (auto& session : _server.players) {
			if (session->connState != ConnectionState::Playing &&
			    session->connState != ConnectionState::WaitingForSpawnChunks)
				continue;
			if (session->dimension != _dimension)
				continue;
			if (session->flushedChunks.contains(chunk))
				continue; // already in flushedSessions
			if (session->sentChunks.contains(chunk)) {
				sentOnlySessions.push_back(session.get());
			}
		}

		// Queue updates for sessions still waiting on the chunk to flush.
		for (auto* session : sentOnlySessions) {
			auto& q = session->pendingBlockChanges[chunk];
			q.insert(q.end(), blockChanges.begin(), blockChanges.end());
		}
		if (flushedSessions.empty())
			continue;

		// Capture chunk ref once for sub-region jobs.
		std::shared_ptr<Chunk> chunkRef = _dimWorld.GetChunk(chunk);

		if (blockChanges.size() == 1) {
			// Single block change: serialise once, raw-copy to every session.
			const PendingBlock& pb = blockChanges[0];
			Packet::SetBlock sb;
			sb.block = { pb.block.type, pb.block.data };
			sb.position = { pb.blockPos.x + (chunk.x * CHUNK_WIDTH), static_cast<int8_t>(pb.blockPos.y), pb.blockPos.z + (chunk.z * CHUNK_WIDTH) };
			// Serialise into a temporary buffer, then send to all sessions.
			NetworkStream tmpStream(-1);
			sb.Serialize(tmpStream);
			const auto& buf = tmpStream.GetRawWriteBuffer();
			for (auto* session : flushedSessions)
				session->stream.WriteRaw(buf.data(), buf.size());
		} else if (blockChanges.size() < 10) {
			// Multi-block packet
			auto formatMultiBlock = [](int8_t _x, int8_t _y, int8_t _z) {
				return (((int16_t(_x) & 0x0F) << 12) | ((int16_t(_z) & 0x0F) << 8) | ((int16_t(_y) & 0xFF)));
			};
			Packet::SetMultipleBlocks smb;
			smb.chunkPosition = { chunk.x, chunk.z };
			for (const auto& pb : blockChanges) {
				smb.blockCoordinates.push_back(static_cast<int16_t>(
				    formatMultiBlock(int8_t(pb.blockPos.x), int8_t(pb.blockPos.y), int8_t(pb.blockPos.z))));
				smb.blockMetadata.push_back(int8_t(pb.block.data));
				smb.blockTypes.push_back(pb.block.type);
			}
			smb.numberOfBlocks = static_cast<int16_t>(smb.blockCoordinates.size());
			NetworkStream tmpStream(-1);
			smb.Serialize(tmpStream);
			const auto& buf = tmpStream.GetRawWriteBuffer();
			for (auto* session : flushedSessions)
				session->stream.WriteRaw(buf.data(), buf.size());
		} else {
			// Sub-region: compression is async per-session via ChunkSender.
			for (auto* session : flushedSessions)
				_server.chunkSender.SendBlockUpdates(*session, chunk, blockChanges, chunkRef);
		}
	}
}
