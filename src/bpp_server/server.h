/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once

#include "addon/addon_manager.h"
#include "base_types.h"
#include "dimensions.h"
#include "entities/entity.h"
#include "entities/entity_player.h"
#include "logger.h"
#include "logstream.h"
#include "numeric_structs.h"
#include "packet_data.h"
#include "platform/platform_caps.h"
#include "player/player_session.h"
#include "world.h"
#include "world/storage/save_manager.h"
#include <atomic>
extern std::atomic<bool> shutdownRequested;

#include "./internet/betacraft_heartbeat.h"
#include "./internet/online_auth.h"
#include "BS_thread_pool.hpp"
#include "chunk_io/chunk_broadcaster.h"
#include "chunk_io/chunk_sender.h"
#include "commands/command_manager.h"
#include "config/config.h"
#include "console/server_console.h"
#include "networking/packets.h"
#include "packet/packet_dispatcher.h"
#include "player_conn/player_session_mp.h"
#include "player_conn/server_pconnstate_manager.h"
#include "runtime.h"
#include "tick_profiler.h"
#include "trackers/entity_tracker.h"
#include "username.h"
#include "world/world_event_broadcaster.h"
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <packet_ids.h>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

class Server {
public:
	Server();
	~Server();

private:
	int serverViewRadius = BPP_DEFAULT_VIEW_DISTANCE;
	int spawnChunkRadius = 5;
	std::atomic<uint16_t> shutdownTimer{ 0 };

public:
#ifdef ONLINE_MODE_AUTHENTICATION
	Authentication auth;
#endif
#ifdef BETACRAFT_HEARTBEAT
	BetacraftHeartbeat betacraftHeartbeat;
	std::chrono::steady_clock::time_point nextHeartbeatSnapshot{};
#endif
	Config config;
	Config gamerulesConfig;
	Runtime gameRuntime;
	ChunkSender chunkSender;

	// Performance metric
	std::atomic<double> averageTickMs{ 0.0 };
	std::atomic<double> averageTickPerSecond{ 0.0 };

	// Whitelist and Operator storage
	bool useWhitelist = false;
	std::vector<std::string> whitelistedUsernames = {};
	std::vector<std::string> operatorUsernames = {};
	std::vector<std::string> bannedUsernames = {};
	std::vector<std::string> bannedIps = {};
	int flushChunkCount = 10;

	// Entity trackers per dimension
	std::unordered_map<Dimension, std::unique_ptr<EntityTracker>> entityTrackers;

	// Apply a function to a tracker with the tracker being the first paramater of the function
	void RunFunctionOnAllTrackers(const std::function<void(EntityTracker*)>& _func);

	void TickEntityTrackers() {
		for (auto& tracker : entityTrackers) {
			tracker.second->Tick();
		}
	}

	void SavePlayer(PlayerSessionMP& _session) {
		if (!_session.entity) {
			Logger::Error << "Failed to save player data for " << _session.username << ": no entity!\n";
			return;
		}
		auto savedNbt = _session.SerializeToNbt();
		gameRuntime.saveManager.SavePlayerNbt(_session.username, savedNbt);
	}

	void SavePlayer(const std::string& _username) {
		auto session = GetSessionByUsername(_username);
		if (!session) {
			Logger::Error << "Failed to save player data for " << _username << "!\n";
			return;
		}
		SavePlayer(*session);
	}

	void DisconnectPlayer(const std::string& _reason, PlayerSessionMP& _session) {
		this->connStateManager.DisconnectPlayer(_session, _reason, *this);
	}

	std::shared_ptr<PlayerSessionMP> GetSessionById(EntityId _entityId) {
		for (const auto& player : players) {
			if (player->entity && player->entity->id == _entityId) {
				return player;
			}
		}
		return nullptr;
	}

	std::shared_ptr<PlayerSessionMP> GetSessionByUsername(const std::string& _username) {
		for (const auto& player : players) {
			if (UsernameEquals(player->username, _username))
				return player;
		}
		return nullptr;
	}

	std::shared_ptr<PlayerSessionMP> GetSessionByIp(const std::string& _ip) {
		for (const auto& player : players) {
			if (!_ip.empty() && player->ipAddress == _ip) {
				return player;
			}
		}
		return nullptr;
	}

	std::string GetUsernameByEntityId(EntityId _id) {
		for (const auto& player : players) {
			if (player->entity && player->entity->id == _id) {
				return player->username;
			}
		}
		return "";
	}

	// Send a message to all players
	void SendGlobalChatMessage(std::string _message) {
		for (auto& other : players) {
			if (!other || other->connState != ConnectionState::Playing)
				continue;
			Packet::ChatMessage reply;
			reply.message = _message;
			reply.Serialize(other->stream);
		}
		Logger::LogStyled(_message);
	}

	std::vector<std::shared_ptr<PlayerSessionMP>>& GetPlayers() noexcept {
		return players;
	}

	std::vector<std::shared_ptr<PlayerSession>> GetPlayersBase() noexcept {
		std::vector<std::shared_ptr<PlayerSession>> result;
		result.reserve(players.size());
		for (auto& sPtr : players) {
			result.push_back(std::dynamic_pointer_cast<PlayerSession>(sPtr));
		}
		return result;
	}

	const AddonManager& GetAddonManager() noexcept {
		return addonManager;
	}

	CommandManager& GetCommandManager() noexcept {
		return commandManager;
	}

	WorldManager* GetWorldForDimension(Dimension _dim) {
		return gameRuntime.GetWorldForDimension(_dim);
	}

	std::unordered_map<Int32_2, std::vector<PendingBlock>>* GetBlockChangeMapForDimension(Dimension _dim) {
		return &blockChangeMap[_dim];
	}

	uint16_t GetMaxPlayers() const noexcept {
		return maximumPlayers;
	}
	bool CanExecuteCommands() const noexcept;
	bool ExecuteCommand(std::string_view _command, PlayerSessionMP* _sender = nullptr) noexcept;

	EntityTracker* GetEntityTrackerForDimension(Dimension _dim) {
		auto it = entityTrackers.find(_dim);
		if (it == entityTrackers.end())
			return nullptr;
		return entityTrackers.at(_dim).get();
	}

	bool TryForceBreak(PlayerSessionMP& _session, WorldManager& _world);
	double GetTickRate() const noexcept {
		return tickRate;
	}
	void SetTickRate(double _rate);
	bool IsTickFrozen() const noexcept {
		return tickFrozen;
	}
	bool ToggleTickFreeze();
	void QueueTickSteps(int _steps);
	enum class SampleMode : uint8_t {
		None,
		Health,
		Entities
	};
	bool BeginTickSample(SampleMode _mode, int _ticks, PlayerSessionMP* _requester);
	void ResetDimensionState(PlayerSessionMP& _session, Dimension _dim);
	void SendEntityToDimension(Dimension _dim, std::shared_ptr<Entity> _entity);
	void SendPlayerToDimension(Dimension _dim, PlayerSessionMP& _session,
	                           std::optional<Vec3> _destOverride = std::nullopt);
	void SetWhitelistEnabled(bool _enabled, bool _persist = true);
	void LoadWhitelist();
	void UnloadWhitelist();
	bool SaveWhitelist();
	bool SaveOperators();
	bool SaveBannedPlayers();
	bool SaveBannedIps();
	void ReloadWhitelist();
	void SaveLevelFile();
	bool PersistGamerule(const std::string& _name);
	void Run();
	void Stop();
	void StopTimeout(float _secondsUntilShutdown);
	void ResetTimeout();
	void ProcessIncoming(PlayerSessionMP& _session);

private:
	bool whitelistLoaded = false;
	friend bool PacketDispatcher::Dispatch(PacketId _packetId, PlayerSessionMP& _session, WorldManager& _sessionWorld,
	                                       Server& _server);
	friend void ChunkBroadcaster::BroadcastBlockChanges(Server& _server,
	                                                    std::unordered_map<Int32_2, std::vector<PendingBlock>>& _changes,
	                                                    Dimension _dimension, WorldManager& _dimWorld);
	friend void WorldEventBroadcaster::BroadcastWorldEvent(Server& _server, PacketData::WorldEvent _eventType,
	                                                       Int3 _position, int32_t _data, Dimension _dimension,
	                                                       PlayerSessionMP* _triggeringSession, double _rangeSq);
	std::vector<std::shared_ptr<PlayerSessionMP>> DisconnectClients();

	void Tick();
	void Startup();
	void AcceptNewPlayers();
	// If every connected player in a dimension is currently sleeping, skip to
	// morning and wake everyone up. Called once per Tick, per dimension.
	void ProcessSleeping(Dimension _dimension);

	// When a player breaks a block
	void OnPlayerBlockBreak(PlayerSessionMP& _session, WorldManager& _world);

	// Config file stuff
	void LoadConfig();
	void LoadGamerules();

	// Chunk-session index helpers
	void IndexAddChunk(PlayerSessionMP& _session, const Int32_2& _pos);
	void IndexRemoveChunk(PlayerSessionMP& _session, const Int32_2& _pos);
	void IndexRemoveSession(PlayerSessionMP& _session);

	// Update our block breaking state
	void UpdateBlockBreaking(PlayerSessionMP& _session, WorldManager& _world);

	// Encodes chunk position + dimension into a single key for chunkSessions.
	// x = chunk X, y = chunk Z, z = dimension id
	static Int32_3 ChunkKey(const Int32_2& _pos, Dimension _dimension) {
		return Int32_3{ _pos.x, _pos.z, int32_t(_dimension) };
	}
	static constexpr int MAX_TICK_CATCH_UP = 5;

	PlayerConnStateManager connStateManager;

	// Global players and dimensional players
	std::vector<std::shared_ptr<PlayerSessionMP>> players;

	// Block change tracking
	std::unordered_map<Dimension, std::unordered_map<Int32_2, std::vector<PendingBlock>>> blockChangeMap;

	// Which sessions currently have a given chunk loaded?
	std::unordered_map<Int32_3, std::vector<PlayerSessionMP*>> chunkSessions;

	bool lastSentRaining = false;

	// Server specifics
	int serverSocket = -1;
	int serverPort = 25565;
	int64_t timeoutSeconds = 60;
	uint16_t maximumPlayers = 20;
	CommandManager commandManager;
	AddonManager addonManager;
	bool stopped = false;
	ServerConsole serverConsole;
	std::vector<std::string> playerNamesSnapshot;
	double tickRate = 20.0;
	std::thread::id commandThreadId{};
	bool tickFrozen = false;
	int pendingTickSteps = 0;
	TickProfiler tickProfiler;
	SampleMode pendingSampleMode = SampleMode::None;
	SampleMode activeSampleMode = SampleMode::None;
	int requestedSampleTicks = 0;
	int collectedSampleTicks = 0;
	std::weak_ptr<PlayerSessionMP> sampleRequester;
	bool sampleToConsole = false;
	bool samplingStartsNextTick = false;

	// Flushes session write buffers off the main tick thread.
	BS::thread_pool<> writePool{ 1 };
};