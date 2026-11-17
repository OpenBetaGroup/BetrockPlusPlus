/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once
#include "../entities/entity_mp_player.h"
#include "../trackers/entity_tracker.h"
#include "player/player_session.h"

class Server;
struct PlayerSessionMP : PlayerSession {
	NetworkStream stream;
	Server* server = nullptr;

	// What block we are currently breaking, if any
	std::optional<PendingBlockBreak> pendingBlockBreak = std::nullopt;

	// What our client is claiming this Tick
	std::optional<Vec3> pendingPosition = std::nullopt;
	std::optional<Vec3> pendingTeleport = std::nullopt;
	std::optional<Int2> pending2dTeleport = std::nullopt;
	bool pendingRespawn = false;

	// Our player entity
	EntityTracker* entityTracker = nullptr;
	bool entityRegistered = false;
	int ticksFloating = 0;

	std::unordered_set<Int32_2> sentChunks;
	std::unordered_set<Int32_2> flushedChunks; // Actually written to stream

	// Chunks that were written to the stream during the last flush() call.
	std::vector<Int32_2> newlyFlushed;

	// Chunks that were unloaded during the last enqueue() call.
	std::vector<Int32_2> newlyUnloaded;

	// Block updates that arrived while the chunk was enqueued but not yet flushed.
	std::unordered_map<Int32_2, std::vector<PendingBlock>> pendingBlockChanges;

	std::string ipAddress;
	std::chrono::steady_clock::time_point lastPacketTime = std::chrono::steady_clock::now();
	static constexpr size_t CHAT_MESSAGE_LIMIT = 3;
	std::array<std::chrono::steady_clock::time_point, CHAT_MESSAGE_LIMIT> recentChatMessageTimes{};
	size_t nextChatMessageTime = 0;
	size_t recentChatMessageCount = 0;

	std::string serverId;
	std::future<bool> pendingAuthFuture;
	std::chrono::steady_clock::time_point authStartTime;

	explicit PlayerSessionMP(int _socket, Runtime& _gameRuntime)
	    : stream(_socket), PlayerSession(_gameRuntime) {}
	~PlayerSessionMP() {
		// So our player entity despawns from the world
		if (entity) {
			auto mpEntity = GetEntityAsMPEntity();
			mpEntity->isDead = true;
			mpEntity->session = nullptr;
		}
		entityTracker = nullptr;
	}

	std::shared_ptr<EntityMPPlayer> GetEntityAsMPEntity() const {
		return std::dynamic_pointer_cast<EntityMPPlayer>(this->entity);
	}

	void Reserved() override {};
};