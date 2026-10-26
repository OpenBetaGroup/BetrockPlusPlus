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
#include <atomic>
extern std::atomic<bool> shutdownRequested;

#include "./internet/betacraft_heartbeat.h"
#include "./internet/online_auth.h"
#include "BS_thread_pool.hpp"
#include "blocks/server_block_behaviors.h"
#include "chunk_io/chunk_broadcaster.h"
#include "chunk_io/chunk_sender.h"
#include "commands/command_manager.h"
#include "config/config.h"
#include "items/item_properties.h"
#include "items/tool_properties.h"
#include "networking/network_stream.h"
#include "networking/packets.h"
#include "packet/handle_packet.h"
#include "packet/packet_dispatcher.h"
#include "player_conn/player_session.h"
#include "player_conn/server_pconnstate_manager.h"
#include "console/server_console.h"
#include "runtime.h"
#include "server_socket.h"
#include "strings/ucs2.h"
#include "username.h"
#include "trackers/entity_tracker.h"
#include "world/world_event_broadcaster.h"
#include "tick_profiler.h"
#include <chrono>
#include <string_view>
#include <memory>
#include <thread>
#include <unordered_set>
#include <vector>

class Server {
public:
	Server();
	~Server();
	void Run();
	void Stop();
	void StopTimeout(float _secondsUntilShutdown);
	void ResetTimeout();
	void ProcessIncoming(PlayerSession& _session);

	Config config;
	Config gamerulesConfig;
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
#endif
	Runtime gameRuntime;
	ChunkSender chunkSender;
	int flushChunkCount = 10;

	void SavePlayer(const std::string& _username) {
		auto PlayerSession = GetSessionByUsername(_username);
		if (!PlayerSession) {
			Logger::Error << "Failed to save player data for " << _username << "!\n";
			return;
		}

		auto savedNbt = PlayerSession->SerializeToNbt();
		gameRuntime.saveManager.SavePlayerNbt(
		    std::string(PlayerSession->username.begin(), PlayerSession->username.end()), savedNbt);
	}

	void DisconnectPlayer(const std::string& _reason, PlayerSession& _session) {
		this->connStateManager.DisconnectPlayer(_session, _reason, *this);
	}

	std::shared_ptr<PlayerSession> GetSessionById(EntityId _entityId) {
		for (const auto& player : players) {
			if (player->entity && player->entity->id == _entityId) {
				return player;
			}
		}
		return nullptr;
	}

	std::shared_ptr<PlayerSession> GetSessionByUsername(const std::string& _username) {
		for (const auto& player : players) {
			if (UsernameEquals(player->username, _username))
				return player;
		}
		return nullptr;
	}

	std::shared_ptr<PlayerSession> GetSessionByIp(const std::string& _ip) {
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

	const std::vector<std::shared_ptr<PlayerSession>>& GetPlayers() noexcept {
		return players;
	}

	const AddonManager& GetAddonManager() noexcept {
		return addonManager;
	}

	WorldManager* GetWorldForDimension(Dimension _dim) {
		return _dim == Dimension::Nether ? &this->gameRuntime.worldHell : &this->gameRuntime.world;
	}

	bool CanExecuteCommands() const noexcept;
	bool ExecuteCommand(std::string_view _command, PlayerSession* _sender = nullptr) noexcept;

	EntityTracker* GetEntityTrackerForDimension(Dimension _dim) {
		return _dim == Dimension::Nether ? &this->hellEntityTracker : &this->overworldEntityTracker;
	}

	bool TryForceBreak(PlayerSession& _session, WorldManager& _world);
	double GetTickRate() const noexcept { return tickRate; }
	void SetTickRate(double _rate);
	bool IsTickFrozen() const noexcept { return tickFrozen; }
	bool ToggleTickFreeze();
	void QueueTickSteps(int _steps);
	enum class SampleMode : uint8_t { None, Health, Entities };
	bool BeginTickSample(SampleMode _mode, int _ticks, PlayerSession* _requester);
	void ResetDimensionState(PlayerSession& _session, Dimension _dim);
	void SendEntityToDimension(Dimension _dim, std::shared_ptr<Entity> _entity);
	void SendPlayerToDimension(Dimension _dim, PlayerSession& _session, std::optional<Vec3> _destOverride = std::nullopt);

	// Entity trackers are so we can send entity updates to players and vice versa.
	EntityTracker overworldEntityTracker;
	EntityTracker hellEntityTracker;
	// Performance metric
	std::atomic<double> averageTickMs{ 0.0 };
	std::atomic<double> averageTickPerSecond{ 0.0 };
	// Whitelist and Operator storage
	bool useWhitelist = false;
	std::vector<std::string> whitelistedUsernames = {};
	std::vector<std::string> operatorUsernames = {};
	// Ban storage. Usernames and IPs are kept in separate lists, mirroring vanilla's
	// banned-players.txt/banned-ips.txt split.
	std::vector<std::string> bannedUsernames = {};
	std::vector<std::string> bannedIps = {};

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

private:
	bool whitelistLoaded = false;
	friend bool PacketDispatcher::Dispatch(PacketId _packetId, PlayerSession& _session, WorldManager& _sessionWorld,
	                                       Server& _server);
	friend void ChunkBroadcaster::BroadcastBlockChanges(Server& _server,
	                                                    std::unordered_map<Int32_2, std::vector<PendingBlock>>& _changes,
	                                                    Dimension _dimension, WorldManager& _dimWorld);
	friend void WorldEventBroadcaster::BroadcastWorldEvent(Server& _server, PacketData::WorldEvent _eventType,
	                                                       Int3 _position, int32_t _data, Dimension _dimension,
	                                                       PlayerSession* _triggeringSession, double _rangeSq);

	void Tick();
	void Startup();
	void AcceptNewPlayers();
	std::vector<std::shared_ptr<PlayerSession>> DisconnectClients();
	// If every connected player in a dimension is currently sleeping, skip to
	// morning and wake everyone up. Called once per Tick, per dimension.
	void ProcessSleeping(Dimension _dimension);

	// When a player breaks a block
	void OnPlayerBlockBreak(PlayerSession& _session, WorldManager& _world);

	// Config file stuff
	void LoadConfig();
	void LoadGamerules();

	// Chunk-session index helpers
	void IndexAddChunk(PlayerSession& _session, const Int32_2& _pos);
	void IndexRemoveChunk(PlayerSession& _session, const Int32_2& _pos);
	void IndexRemoveSession(PlayerSession& _session);

	// Update our block breaking state
	void UpdateBlockBreaking(PlayerSession& _session, WorldManager& _world);

	// Encodes chunk position + dimension into a single key for chunkSessions.
	// x = chunk X, y = chunk Z, z = dimension id
	static Int32_3 ChunkKey(const Int32_2& _pos, Dimension _dimension) {
		return Int32_3{ _pos.x, _pos.z, int32_t(_dimension) };
	}
	static constexpr int MAX_TICK_CATCH_UP = 5;

	PlayerConnStateManager connStateManager;

	// Global players and dimensional players
	std::vector<std::shared_ptr<PlayerSession>> players;

	// Block change tracking
	std::unordered_map<Int32_2, std::vector<PendingBlock>> chunkBlockChanges;
	std::unordered_map<Int32_2, std::vector<PendingBlock>> chunkBlockChangesHell;

	// Which sessions currently have a given chunk loaded?
	std::unordered_map<Int32_3, std::vector<PlayerSession*>> chunkSessions;

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
	std::weak_ptr<PlayerSession> sampleRequester;
	bool sampleToConsole = false;
	bool samplingStartsNextTick = false;

	// Flushes session write buffers off the main tick thread.
	BS::thread_pool<> writePool{ 1 };
};