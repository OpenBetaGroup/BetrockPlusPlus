/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "addon/addon_impl.h"
#include "addon/addon_manager.h"
#include "base_types.h"
#include "config/list_parser.h"
#include "dimensions.h"
#include "gamerules.h"
#include "logger.h"
#include "packet/packet_utils.h"
#include "trackers/inventory_tracker.h"
#include "world.h"
#include "teleporter.h"
#include "commands/command.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <future>
#include <climits>
#include <cstdint>
#include <string>

#include "platform/platform_caps.h"
#if defined(BPP_HAS_POSIX_SOCKETS)
#include <fcntl.h>
#include <iomanip>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include "../bpp_utilities/compression_test.h"
#include "../bpp_utilities/gen_test.h"

#include "server.h"
#include "version.h"
#include <chrono>

Server::Server() : config("server.properties"), gamerulesConfig("gamerules.txt"), gameRuntime(), addonManager(this) {
	ServerBlock::Initialize();
	LoadConfig();

	serverSocket = ServerSocketManager::CreateServerSocket(serverPort);
	if (serverSocket < 0) {
		Logger::Error << "**** FAILED TO CREATE SERVER SOCKET!" << "\n";
		exit(1);
	}
	Logger::Info << "Server initialized on port " << serverPort << "\n";
	// TODO: If seed is 0, have it generate a new seed
	gameRuntime.Init(config.GetAsString("level-name", "world"), config.GetAsString("level-seed", "0"),
	                 config.GetAsNumber("view-distance", 8));
	commandManager.Init(this);
}

bool Server::CanExecuteCommands() const noexcept {
	if (std::this_thread::get_id() != commandThreadId)
		return false;
	return !stopped && !shutdownRequested.load();
}

bool Server::ExecuteCommand(std::string_view command, PlayerSession* sender) noexcept {
	if (!CanExecuteCommands()) return false;

	try {
		std::shared_ptr<PlayerSession> owned;
		if (sender) {
			for (const auto& player : players) {
				if (player.get() == sender) {
					owned = player;
					break;
				}
			}
			if (!owned || !owned->stream.IsConnected() || owned->connState != ConnectionState::Playing ||
			    !owned->entity)
				return false;
		}

		CommandContext ctx;
		ctx.sender = sender;
		ctx.session = sender;
		ctx.world = sender ? GetWorldForDimension(sender->dimension) : &gameRuntime.world;
		ctx.server = this;

		return commandManager.Parse(command, ctx);
	} catch (...) { return false; }
}

Server::~Server() {
	this->Stop();
}

void Server::SendEntityToDimension(Dimension _dim, std::shared_ptr<Entity> _entity) {
	// Remove our entity from our watcher
	Dimension oldDim = _entity->dim;
	EntityId oldId = _entity->id;
	if (oldDim == _dim)
		return;

	// Remove the entity from the world's entity managers
	WorldManager* world = GetWorldForDimension(oldDim);
	WorldManager* newWorld = GetWorldForDimension(_dim);
	world->entityManager.RemoveEntity(_entity->id);

	// Rebind entity
	_entity->isDead = false;
	newWorld->entityManager.AddEntity(_entity, oldId);
}

void Server::ResetDimensionState(PlayerSession& _session, Dimension _dim) {
	IndexRemoveSession(_session);
	_session.dimension = _dim;
	_session.flushedChunks.clear();
	_session.sentChunks.clear();
	_session.pendingBlockChanges.clear();
	_session.newlyFlushed.clear();
	_session.newlyUnloaded.clear();
	_session.entityTracker = _dim == Dimension::Overworld ? &overworldEntityTracker : &hellEntityTracker;
	chunkSender.inFlight.erase(&_session);
	chunkSender.subRegionFlight.erase(&_session);
}

void Server::SendPlayerToDimension(Dimension _dim, PlayerSession& _session, std::optional<Vec3> _destOverride) {
	if (_dim == _session.dimension)
		return;

	ResetDimensionState(_session, _dim);

	Vec3 dest = _session.entity->position;
	if (_destOverride) {
		dest = *_destOverride;
	} else if (_dim == Dimension::Nether) {
		dest.x /= 8.0;
		dest.z /= 8.0;
	} else {
		dest.x *= 8.0;
		dest.z *= 8.0;
	}
	_session.entity->Teleport(dest);
	_session.pendingPortalPlacement = !_destOverride.has_value();;
	_session.portalAreaLoaded = false;

	Packet::Respawn pkt;
	pkt.dimension = _dim;
	pkt.Serialize(_session.stream);
	_session.connState = ConnectionState::WaitingForSpawnChunks;
	_session.position.pos = _session.entity->position;
	_session.pendingTeleport = _session.entity->position;
	_session.pendingPosition.reset();
	SendEntityToDimension(_dim, _session.entity);

	Packet::SetHealth sh;
	sh.health = _session.entity->health;
	sh.Serialize(_session.stream);
	PacketUtilities::SendInventory(_session, 0, _session.inventory);

	_session.entity->wasMetadataUpdated = true;
	_session.inventory.activeHotbarSlot = 0;
}

void Server::IndexAddChunk(PlayerSession& _session, const Int32_2& _pos) {
	auto& vec = chunkSessions[ChunkKey(_pos, _session.dimension)];
	// Avoid duplicates (should never happen, but be safe)
	for (auto* p : vec)
		if (p == &_session)
			return;
	vec.push_back(&_session);
}

void Server::IndexRemoveChunk(PlayerSession& _session, const Int32_2& _pos) {
	auto it = chunkSessions.find(ChunkKey(_pos, _session.dimension));
	if (it == chunkSessions.end())
		return;
	auto& vec = it->second;
	vec.erase(std::remove(vec.begin(), vec.end(), &_session), vec.end());
	if (vec.empty())
		chunkSessions.erase(it);
}

void Server::IndexRemoveSession(PlayerSession& _session) {
	for (const auto& pos : _session.flushedChunks)
		IndexRemoveChunk(_session, pos);
}

void Server::LoadConfig() {
	if (!config.LoadFromDisk()) {
		config.Overwrite({
		    { "level-name", "world" },
		    { "view-distance", "10" },
		    { "white-list", "false" },
		    //{"server-ip", ""},
		    //{"motd", "A Betrock++ Server"},
		    //{"pvp", "true"},
		    { "level-seed", std::to_string(std::mt19937(std::random_device()())()) },
		    { "spawn-animals", "true" },
		    { "spawn-monsters", "true" },
		    { "server-port", "25565" },
		    //{"allow-nether",true},
		    { "max-players", "20" },
		    { "online-mode", "false" },
		    { "allow-floodgate-prefix", "false" },
#ifdef BETACRAFT_HEARTBEAT
		    { "betacraft-heartbeat", "false" },
		    { "betacraft-name", "A Betrock++ Server" },
		    { "betacraft-description", "" },
		    { "betacraft-socket", "" },
		    { "betacraft-private-key", "" },
		    { "betacraft-category", "beta" },
		    { "betacraft-game-version", "b1.7.3" },
		    { "betacraft-protocol", "beta_14" },
		    { "betacraft-v1-version", "b1.7.3" },
		    { "betacraft-send-players", "true" },
		    { "betacraft-icon", "icon.png" },
#endif
		    //{"allow-flight","false"}
		});
		config.SaveToDisk();
	}
	//chunkDistance = config.GetAsNumber<int32_t>("view-distance");
	serverPort = config.GetAsNumber<int32_t>("server-port", 25565);
#ifdef ONLINE_MODE_AUTHENTICATION
	auth.onlineMode = config.GetAsBoolean("online-mode", false);
#endif
#ifdef BETACRAFT_HEARTBEAT
	betacraftHeartbeat.Load(config, serverPort);
#endif
	//motd = config.GetAsString("motd");
	const bool spawnAnimals = config.GetAsBoolean("spawn-animals", true);
	Gamerule::Set(Gamerule::Id::SpawnMobs, spawnAnimals);
	const bool spawnMonsters = config.GetAsBoolean("spawn-monsters", true);
	Gamerule::Set(Gamerule::Id::SpawnMonsters, spawnMonsters);
	maximumPlayers = config.GetAsNumber<uint16_t>("max-players", 20);
	//maximumThreads = config.GetAsNumber<int32_t>("max-generator-threads");
	useWhitelist = config.GetAsBoolean("white-list");
	operatorUsernames = ListParser::Read(ListParser::Target::Operator);
	// Bans are always enforced, unlike the whitelist, so we load them unconditionally
	bannedUsernames = ListParser::Read(ListParser::Target::BannedPlayers);
	bannedIps = ListParser::Read(ListParser::Target::BannedIps);
	if (useWhitelist) {
		LoadWhitelist();
		Logger::Info << "Whitelist enabled!\n";
	}
	// Loaded last so gamerules.txt takes priority over the legacy spawn-animals/spawn-monsters keys
	LoadGamerules();
}

void Server::LoadGamerules() {
	if (!gamerulesConfig.LoadFromDisk()) {
		// No gamerules.txt yet, create it from the current (default) values
		Gamerule::WriteToConfig(gamerulesConfig);
		gamerulesConfig.SaveToDisk();
		return;
	}
	// Add any gamerules that are missing from the file (e.g. ones added in an update)
	if (Gamerule::ReadFromConfig(gamerulesConfig)) {
		Gamerule::WriteToConfig(gamerulesConfig);
		gamerulesConfig.SaveToDisk();
	}
}

bool Server::PersistGamerule(const std::string& _name) {
	if (Gamerule::ToId(_name) == Gamerule::Id::Invalid)
		return false;
	gamerulesConfig.Set(_name, Gamerule::ValueToString(Gamerule::GetRaw(_name)));
	if (!gamerulesConfig.SaveKeyToDisk(_name)) {
		Logger::Error << "Failed to persist gamerule '" << _name << "' to gamerules.txt\n";
		return false;
	}
	return true;
}

void Server::LoadWhitelist() {
	if (whitelistLoaded)
		return;
	whitelistedUsernames = ListParser::Read(ListParser::Target::Whitelist);
	whitelistLoaded = true;
}

void Server::UnloadWhitelist() {
	whitelistedUsernames.clear();
	whitelistedUsernames.shrink_to_fit();
	whitelistLoaded = false;
}

void Server::SetWhitelistEnabled(bool _enabled, bool _persist) {
	if (_enabled)
		LoadWhitelist();
	const bool changed = useWhitelist != _enabled;
	useWhitelist = _enabled;
	if (!_enabled)
		UnloadWhitelist();
	if (changed)
		Logger::Info << (useWhitelist ? "Whitelist enabled!\n" : "Whitelist disabled!\n");
	if (!_persist)
		return;
	config.Set("white-list", useWhitelist ? "true" : "false");
	if (!config.SaveKeyToDisk("white-list"))
		Logger::Error << "Failed to persist white-list to server.properties\n";
}

bool Server::SaveWhitelist() {
	if (!whitelistLoaded)
		return true;
	return ListParser::Write(whitelistedUsernames, ListParser::Target::Whitelist);
}

bool Server::SaveOperators() {
	return ListParser::Write(operatorUsernames, ListParser::Target::Operator);
}

bool Server::SaveBannedPlayers() {
	return ListParser::Write(bannedUsernames, ListParser::Target::BannedPlayers);
}

bool Server::SaveBannedIps() {
	return ListParser::Write(bannedIps, ListParser::Target::BannedIps);
}

void Server::ReloadWhitelist() {
	if (!useWhitelist)
		return;
	whitelistedUsernames = ListParser::Read(ListParser::Target::Whitelist);
	whitelistLoaded = true;
}

void Server::Startup() {
	auto startupStart = std::chrono::steady_clock::now();
	Logger::Info << "Initializing server startup.. \n";

	// Init auth
#ifdef ONLINE_MODE_AUTHENTICATION
	if (!auth.onlineMode) {
		Logger::Warning << "**** SERVER IS RUNNING IN OFFLINE/INSECURE MODE!\n"
		                    << "The server will make no attempt to authenticate usernames. Beware.\n"
		                    << "While this makes the game possible to play without internet access,\n"
		                    << "it also opens up the ability for hackers to connect with any username they choose.\n"
		                    << "To change this, set \"online-mode\" to \"true\" in the server.properties file.\n";
	}
#endif

	// Load addons
	addonManager.Load();

	// Setup the block callback so we can send it to clients
	auto makeBlockUpdateCallback = [this](Dimension _dimensionId, auto& _blockChangeMap) {
		return [this, _dimensionId, &_blockChangeMap](PendingBlock _pendingBlock, Int32_2 _chunkPos) {
			auto idxIt = chunkSessions.find(ChunkKey(_chunkPos, _dimensionId));
			bool anyInterested = (idxIt != chunkSessions.end() && !idxIt->second.empty());
			if (!anyInterested) {
				for (auto& session : players) {
					if (session->dimension == _dimensionId && session->sentChunks.contains(_chunkPos)) {
						anyInterested = true;
						break;
					}
				}
			}
			if (!anyInterested)
				return;

			PendingBlock pendingNew = _pendingBlock;
			pendingNew.blockPos = { _pendingBlock.blockPos.x & 15, _pendingBlock.blockPos.y,
				                    _pendingBlock.blockPos.z & 15 };
			_blockChangeMap[_chunkPos].push_back(pendingNew);
		};
	};

	auto registerEntityTrackerCallbacks = [this](EntityTracker& _entityTracker, EntityManager& _entityManager) {
		_entityManager.onEntitySpawn = [&_entityTracker](std::shared_ptr<Entity> _entity) {
			if (_entity->type == EntityType::PLAYER) {
				_entityTracker.AddPlayer(_entity.get());
				return;
			}
			_entityTracker.TrackEntity(_entity.get());
		};
		_entityManager.onEntityDespawn = [&_entityTracker](std::shared_ptr<Entity> _entity) {
			if (_entity->type == EntityType::PLAYER) {
				_entityTracker.RemovePlayer(_entity.get());
				return;
			}
			_entityTracker.UntrackEntity(_entity.get());
		};
		_entityTracker.server = this;
	};

	auto registerExplosionCallback = [this](WorldManager& _world, Dimension _dimension) {
		// Sent by position, not by who can see the exploder: TNT and beds explode with no exploder
		_world.onExplosion = [this, _dimension](Vec3 _pos, float _size, std::unordered_set<Int3>& _blockPositions,
		                                        Entity* /*_exploder*/) {
			WorldEventBroadcaster::BroadcastExplosion(*this, _pos, _size, _blockPositions, _dimension);
		};
	};

	auto registerSpawnProtection = [this](WorldManager& _world) {
		_world.canModifyBlock = [this, &_world](Entity& _entity, Int3 _pos) {
			auto* player = dynamic_cast<EntityMPPlayer*>(&_entity);
			return !player || !player->session || IsValidActionInSpawnRadius(*player->session, *this, _pos, _world);
		};
	};
	registerSpawnProtection(gameRuntime.world);
	registerSpawnProtection(gameRuntime.worldHell);

	gameRuntime.world.onBlockUpdate = makeBlockUpdateCallback(Dimension::Overworld, chunkBlockChanges);
	gameRuntime.worldHell.onBlockUpdate = makeBlockUpdateCallback(Dimension::Nether, chunkBlockChangesHell);

	gameRuntime.world.onWorldEvent = [this](PacketData::WorldEvent _eventType, Int3 _position, int32_t _data,
	                                        PlayerSession* _triggeringSession) {
		WorldEventBroadcaster::BroadcastWorldEvent(*this, _eventType, _position, _data, Dimension::Overworld,
		                                           _triggeringSession);
	};
	gameRuntime.worldHell.onWorldEvent = [this](PacketData::WorldEvent _eventType, Int3 _position, int32_t _data,
	                                            PlayerSession* _triggeringSession) {
		WorldEventBroadcaster::BroadcastWorldEvent(*this, _eventType, _position, _data, Dimension::Nether,
		                                           _triggeringSession);
	};

	gameRuntime.world.onNotePlay = [this](Int3 _pos, int8_t _instrumentState, int8_t _instrumentDirection) {
		WorldEventBroadcaster::BroadcastNoteEvent(*this, _pos, _instrumentState, _instrumentDirection,
		                                          Dimension::Overworld);
	};
	gameRuntime.worldHell.onNotePlay = [this](Int3 _pos, int8_t _instrumentState, int8_t _instrumentDirection) {
		WorldEventBroadcaster::BroadcastNoteEvent(*this, _pos, _instrumentState, _instrumentDirection,
		                                          Dimension::Nether);
	};

	registerEntityTrackerCallbacks(overworldEntityTracker, gameRuntime.world.entityManager);
	registerEntityTrackerCallbacks(hellEntityTracker, gameRuntime.worldHell.entityManager);

	registerExplosionCallback(gameRuntime.world, Dimension::Overworld);
	registerExplosionCallback(gameRuntime.worldHell, Dimension::Nether);

	// Get spawn ready
	int spawnChunkDistance = this->spawnChunkRadius;
	int totalSpawnChunks = (spawnChunkDistance + spawnChunkDistance + 1) * (spawnChunkDistance + spawnChunkDistance + 1);
	Logger::Info << "Server spawn is "
	                    << Int2(int(gameRuntime.world.spawnPoint.x), int(gameRuntime.world.spawnPoint.z)) << "\n";

	Logger::Info << "Preparing spawn chunks..\n";
	if (Gamerule::Get<bool>(Gamerule::Id::SpawnChunks)) {
		// Push every single spawn chunk to get ready for generation
		std::unordered_set<Int32_2> wanted;
		for (int dx = -spawnChunkDistance; dx <= spawnChunkDistance; dx++) {
			for (int dz = -spawnChunkDistance; dz <= spawnChunkDistance; dz++) {
				Int32_2 pos = { (gameRuntime.world.spawnPoint.x >> 4) + dx, (gameRuntime.world.spawnPoint.z >> 4) + dz };
				wanted.insert(pos);
			}
		}

		// Actually request chunks
		for (auto pos : wanted) {
			if (!gameRuntime.world.chunks.contains(pos)) {
				auto c = std::make_shared<Chunk>();
				c->spawnChunk = true;
				c->cpos = pos;
				gameRuntime.world.chunks.emplace(pos, std::move(c));
			}
			if (!gameRuntime.worldHell.chunks.contains(pos)) {
				auto c = std::make_shared<Chunk>();
				c->spawnChunk = true;
				c->cpos = pos;
				gameRuntime.worldHell.chunks.emplace(pos, std::move(c));
			}
		}

		// Chunks are ready to load at this point.
		auto loadSpawnChunks = [totalSpawnChunks, spawnChunkDistance](WorldManager& _world) {
			auto start = std::chrono::steady_clock::now();
			int loadedChunks = 0;
			while (true) {
				loadedChunks = 0;
				// Force gen these chunks AS FAST AS POSSIBLE
				_world.PumpPipeline({});
				_world.pool.wait();
				_world.DrainGenQueue();
				_world.regionManager->iopool.wait();
				_world.DrainLoadQueue();
				_world.PopulateReady(INT_MAX);
				_world.lightManager.ProcessLightQueue(_world);

				for (int dx = -spawnChunkDistance; dx <= spawnChunkDistance; dx++) {
					for (int dz = -spawnChunkDistance; dz <= spawnChunkDistance; dz++) {
						Int32_2 p{ (_world.spawnPoint.x >> 4) + dx, (_world.spawnPoint.z >> 4) + dz };
						auto it = _world.chunks.find(p);
						if (it != _world.chunks.end() && it->second->state.load() >= ChunkState::Generated)
							loadedChunks++;
					}
				}

				// Update load percentage every second
				if (std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count() >= 1.0f) {
					int percentLoaded = int((float(loadedChunks) / float(totalSpawnChunks)) * 100.0f);
					Logger::Info << "Loading spawn.. " << percentLoaded << "%\n";
					start = std::chrono::steady_clock::now();
				}

				// Have we loaded all the spawn chunks?
				if (loadedChunks >= totalSpawnChunks)
					break;
			}
			Logger::Info << "Loading spawn.. 100%\n";
		};

		// Benchmark gen speed
		//ChunkBenchmark::PerformChunkGenSpeedTest(gameRuntime.world);

		Logger::Info << "Loading spawn chunks for Overworld: (" << totalSpawnChunks << ")\n";
		loadSpawnChunks(gameRuntime.world);

		/*
		std::vector<Chunk*> chunks;
		for (auto& c : gameRuntime.world.chunks) {
			chunks.push_back(c.second.get());
		}
		auto result = ChunkBenchmark::Benchmark(chunks, 100);
		ChunkBenchmark::Print(result);
		*/

		Logger::Info << "Loading spawn chunks for Hell: (" << totalSpawnChunks << ")\n";
		loadSpawnChunks(gameRuntime.worldHell);
	}

	float startupSeconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - startupStart).count();
	Logger::Info << "Startup Complete. (" << std::setprecision(4) << startupSeconds << "s)\n";
#ifdef BETACRAFT_HEARTBEAT
	betacraftHeartbeat.Start();
#endif
}

void Server::Run() {
	commandThreadId = std::this_thread::get_id();
	Startup();
	serverConsole.UpdatePlayerNames(playerNamesSnapshot);
	serverConsole.Start(commandManager);
	using Clock = std::chrono::steady_clock;

	std::chrono::nanoseconds avgTotalTickDuration{ 0 };
	int avgTickCount = 0;
	auto tpsWindowStart = Clock::now();
	int tpsWindowTicks = 0;
	uint64_t ticks = 0;
	auto nextTickTime = Clock::now();

	// Main Tick loop
	// Heavily based on https://github.com/Minestom/Minestom/blob/59406d5b54d5221df85f381f204fbc07fd861a43/src/main/java/net/minestom/server/thread/TickSchedulerThread.java
	while (!shutdownRequested.load()) {
		auto tickStart = Clock::now();
		Tick();
		auto tickEnd = Clock::now();

		// Sample and print average Tick data
		avgTotalTickDuration += (tickEnd - tickStart);
		++avgTickCount;
		++tpsWindowTicks;

		const double interval = GetTickRate() * 2.0;
		// Note: std::fmod is gross, but we kinda have to do this :p
		if (interval > 0.0 && std::fmod(static_cast<double>(ticks), interval) < 1e-9) {
			const auto now = Clock::now();
			const double wallSec = std::chrono::duration<double>(now - tpsWindowStart).count();

			averageTickMs = std::chrono::duration<double, std::milli>(avgTotalTickDuration).count() /
			double(avgTickCount);
			// Skip the first sample (ticks == 0): its window only covers one tick
			if (ticks > 0 && wallSec > 0.0)
				averageTickPerSecond = double(tpsWindowTicks) / wallSec;

			tpsWindowStart = now;
			tpsWindowTicks = 0;
			avgTotalTickDuration = std::chrono::nanoseconds{ 0 };
			avgTickCount = 0;
		}

		++ticks;
		const auto period = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / tickRate));
		nextTickTime += period;
		// Slice the wait so shutdownRequested is observed even if libc++
		// restarts sleep_until() after EINTR (musl + libcurl).
		while (!shutdownRequested.load() && Clock::now() < nextTickTime) {
			auto remaining = nextTickTime - Clock::now();
			auto slice = remaining < std::chrono::milliseconds(10) ? remaining : std::chrono::milliseconds(10);
			std::this_thread::sleep_for(slice);
		}
		if (shutdownRequested.load())
			break;

		// Check if the server can not keep up with the tickrate
		// if it gets too far behind, reset the ticks & baseTime
		// to avoid running too many ticks at once
		if (Clock::now() > nextTickTime + MAX_TICK_CATCH_UP * period) {
			auto overshoot = std::chrono::duration<double, std::milli>(Clock::now() - nextTickTime).count();
			nextTickTime = Clock::now();
			Logger::Warning << "Can't keep up with ticks! (tick avg " << averageTickMs.load() << "ms, overshoot "
			                    << overshoot << "ms, budget "
			                    << std::chrono::duration<double, std::milli>(period).count() << "ms)\n";
		}
	}

	// Wait for any in-flight write flushes before saving and cleaning up
	while (!writePool.wait_for(std::chrono::milliseconds(100))) {
		if (shutdownRequested.load())
			break;
	}

	// Shutdown was requested. Save and clean up on the main thread
	serverConsole.Stop();
	Stop();
	shutdownRequested.store(false); // Unblock the ctrl handler thread
}

void Server::Stop() {
	if (stopped)
		return;
	stopped = true;
	serverConsole.Stop();

#ifdef BETACRAFT_HEARTBEAT
	betacraftHeartbeat.Stop();
#endif
	Logger::Info << "Server shutting down...\n";
	for (auto& session : players) {
		connStateManager.DisconnectPlayer(*session, "Server Closed", *this);
		session->stream.FlushWriteBufferBlocking();
	}
	ServerSocketManager::CloseSocket(serverSocket);
	gameRuntime.world.Shutdown();
	gameRuntime.worldHell.Shutdown();

	// Save our level file
	SaveLevelFile();

	// Save operator, whitelist, and ban updates
	SaveWhitelist();
	SaveOperators();
	SaveBannedPlayers();
	SaveBannedIps();
}

void Server::SaveLevelFile() {
	LevelData& curLevelData = gameRuntime.saveManager.GetLevelData();
	curLevelData.randomSeed = gameRuntime.world.seed;
	curLevelData.spawnPoint = gameRuntime.world.spawnPoint;
	curLevelData.time = gameRuntime.world.elapsedTicks;
	curLevelData.raining = gameRuntime.world.weatherSystem.isRaining;
	curLevelData.rainTime = gameRuntime.world.weatherSystem.rainTimer;
	curLevelData.thundering = gameRuntime.world.weatherSystem.isThundering;
	curLevelData.thunderTime = gameRuntime.world.weatherSystem.thunderTimer;
	gameRuntime.saveManager.SaveLevelFile(curLevelData);
}

void Server::AcceptNewPlayers() {
    constexpr int K_MAX_ACCEPTS_PER_TICK = 16;
    for (int i = 0; i < K_MAX_ACCEPTS_PER_TICK; ++i) {
        std::string clientIp;
        auto clientSocket = ServerSocketManager::CreateClientSocket(serverSocket, &clientIp);
        if (clientSocket < 0)
            return; // queue is empty
        auto session = std::make_shared<PlayerSession>(clientSocket, gameRuntime);
        if (players.size() >= maximumPlayers) {
            DisconnectPlayer("Server is full!", *session);
            continue; // keep draining so rejected clients get their kick message promptly
        }
        session->ipAddress = clientIp;
        session->server = this;
        players.push_back(std::move(session));
    }
}

void Server::StopTimeout(float _secondsUntilShutdown) {
	// Garbage or "now" means stop right away. (A timer of 0 means "no shutdown pending", so it can't express this.)
	if (!std::isfinite(_secondsUntilShutdown) || _secondsUntilShutdown <= 0.0f) {
		shutdownRequested.store(true);
		return;
	}
	const double ticks = std::ceil(static_cast<double>(_secondsUntilShutdown) * GetTickRate());
	shutdownTimer.store(static_cast<uint16_t>(std::clamp(ticks, 2.0, static_cast<double>(UINT16_MAX))));
}

void Server::ResetTimeout() {
	shutdownTimer.store(0);
}

void Server::SetTickRate(double _rate) {
	using Clock = std::chrono::steady_clock;
	if (!std::isfinite(_rate) || _rate <= 0.0)
		return;
	const auto period = std::chrono::duration<double>(1.0 / _rate);
	const auto minPeriod = std::chrono::duration<double>(Clock::duration{ 1 });
	const auto maxPeriod = std::chrono::duration<double>(Clock::duration::max());
	if (period < minPeriod || period > maxPeriod)
		return;
	tickRate = _rate;
}

bool Server::ToggleTickFreeze() {
	tickFrozen = !tickFrozen;
	if (!tickFrozen)
		pendingTickSteps = 0;
	gameRuntime.world.SetFrozen(tickFrozen);
	gameRuntime.worldHell.SetFrozen(tickFrozen);
	return tickFrozen;
}

void Server::QueueTickSteps(int _steps) {
	if (_steps > 0 && tickFrozen && pendingTickSteps <= INT_MAX - _steps)
		pendingTickSteps += _steps;
}

bool Server::BeginTickSample(SampleMode mode, int ticks, PlayerSession* requester) {
	if (pendingSampleMode != SampleMode::None || activeSampleMode != SampleMode::None)
		return false;
	sampleRequester.reset();
	sampleToConsole = requester == nullptr;
	if (requester) {
		for (const auto& session : players) {
			if (session.get() == requester && session->connState == ConnectionState::Playing) {
				sampleRequester = session;
				break;
			}
		}
		if (sampleRequester.expired()) {
			sampleToConsole = false;
			return false;
		}
	}
	pendingSampleMode = mode;
	requestedSampleTicks = ticks;
	collectedSampleTicks = 0;
	samplingStartsNextTick = true;
	return true;
}
void Server::Tick() {
	// Wait for the previous tick's write flushes to finish
	using Clock = std::chrono::steady_clock;
	Clock::time_point totalStart{};
	if (samplingStartsNextTick) {
		tickProfiler.Reset();
		tickProfiler.EnableEntityProfiling(pendingSampleMode == SampleMode::Entities);
		activeSampleMode = pendingSampleMode;
		pendingSampleMode = SampleMode::None;
		samplingStartsNextTick = false;
		collectedSampleTicks = 0;
	}
	const bool sampling = activeSampleMode != SampleMode::None;
	if (sampling)
		totalStart = Clock::now();
	auto offTickStart = Clock::now();
	writePool.wait();

	std::vector<std::string> queuedCommands;
	serverConsole.DrainCommands(queuedCommands);

	for (const auto& command : queuedCommands) {
		if (shutdownRequested.load())
			break;
		ExecuteCommand(command, nullptr);
	}

	if (sampling)
		tickProfiler.Record(TickTask::OffTickTasks, Clock::now() - offTickStart);
	std::chrono::nanoseconds autosaveElapsed{ 0 };
	auto networkStart = Clock::now();
	AcceptNewPlayers();
	chunkSender.BeginTick();

	std::vector<ClientPosition> overworldPositions;
	std::vector<ClientPosition> netherPositions;
	for (auto& session : players) {
		session->stream.DrainToBuffer();
		if (session->connState == ConnectionState::WaitingForSpawnChunks ||
		    session->connState == ConnectionState::Playing) {
			// Process our packets
			ProcessIncoming(*session);

			// Register with the correct dimension
			if (session->dimension == Dimension::Nether)
				netherPositions.push_back(session->position);
			else
				overworldPositions.push_back(session->position);

			// Update our break state
			this->UpdateBlockBreaking(*session, *GetWorldForDimension(session->dimension));


		}
		connStateManager.HandleConnectionState(*session, *this);

		// Drain chunk-session index updates that ChunkSender recorded
		for (const auto& pos : session->newlyFlushed)
			IndexAddChunk(*session, pos);
		session->newlyFlushed.clear();
		for (const auto& pos : session->newlyUnloaded)
			IndexRemoveChunk(*session, pos);
		session->newlyUnloaded.clear();
	}
	const bool advanceSimulation = !tickFrozen || pendingTickSteps > 0;
	if (advanceSimulation && gameRuntime.world.tickScheduler.currentTick % 40 == 0) {
		for (const auto& session : players) {
			if (session->connState != ConnectionState::WaitingForSpawnChunks &&
			    session->connState != ConnectionState::Playing)
				continue;
			auto saveStart = Clock::now();
			SavePlayer(session->username);
			const auto saveElapsed = Clock::now() - saveStart;
			autosaveElapsed += saveElapsed;
			if (sampling)
				tickProfiler.Record(TickTask::Autosave, saveElapsed);
		}
	}
	const auto networkInputElapsed = Clock::now() - networkStart - autosaveElapsed;
	auto inventoryStart = Clock::now();
	InventoryTracker::Tick(*this);
	if (advanceSimulation) {
		const bp_server_tick_event event{};
		addonManager.Broadcast(&bp_addon_events::serverTick, event);
	}
	if (sampling)
		tickProfiler.Record(TickTask::OffTickTasks, Clock::now() - inventoryStart);
	gameRuntime.world.Tick(overworldPositions, advanceSimulation, sampling ? &tickProfiler : nullptr);
	gameRuntime.world.Update(overworldPositions);
	gameRuntime.worldHell.Tick(netherPositions, advanceSimulation, sampling ? &tickProfiler : nullptr);
	gameRuntime.worldHell.Update(netherPositions);

	if (advanceSimulation && pendingTickSteps > 0)
		--pendingTickSteps;
	if (advanceSimulation) {
		// If everyone in a dimension is asleep, skip the night.
		// (Beds can currently only be used in the Overworld, but this stays generic.)
		ProcessSleeping(Dimension::Overworld);
		ProcessSleeping(Dimension::Nether);
	}

	// Send all of the block changes that have accumulated since the last Tick, then clear the list.
	std::unordered_map<Int32_2, std::vector<PendingBlock>> localBlockChanges;
	std::unordered_map<Int32_2, std::vector<PendingBlock>> localBlockChangesHell;
	localBlockChanges.swap(chunkBlockChanges);
	localBlockChangesHell.swap(chunkBlockChangesHell);

	auto networkOutputStart = Clock::now();

	// Entity trackers
	overworldEntityTracker.Tick();
	hellEntityTracker.Tick();

	// Dispatch block changes
	ChunkBroadcaster::BroadcastBlockChanges(*this, localBlockChanges, Dimension::Overworld, gameRuntime.world);
	ChunkBroadcaster::BroadcastBlockChanges(*this, localBlockChangesHell, Dimension::Nether, gameRuntime.worldHell);

	// Send weather updates
	bool raining = gameRuntime.world.weatherSystem.IsRaining();
	if (raining != lastSentRaining) {
		lastSentRaining = raining;
		Packet::GameEvent pkt;
		pkt.eventId = raining ? PacketData::GameEvent::START_RAINING : PacketData::GameEvent::STOP_RAINING;
		overworldEntityTracker.SendPacketToAllPlayers(pkt);
	}

	// Disconnect timed-out clients; the removed sessions are returned so we
	// can send their last packets (e.g. a disconnect reason) before they die
	std::vector<std::shared_ptr<PlayerSession>> removedSessions = this->DisconnectClients();

	// Flush pending outgoing data on the write thread so socket I/O never
	// holds up the main tick thread
	std::vector<std::future<void>> flushFutures;
	for (auto& session : players) {
		if (session->stream.GetRawWriteBuffer().empty())
			continue;
		auto& sessionRef = session;
		flushFutures.push_back(writePool.submit_task([sessionRef]() { sessionRef->stream.FlushWriteBuffer(); }));
	}

	// Sessions removed this tick get one final synchronous flush (their
	// pending data is small) before their shared_ptr drops and they are destroyed
	for (auto& session : removedSessions)
		session->stream.FlushWriteBuffer();
	if (sampling)
		tickProfiler.Record(TickTask::Network, networkInputElapsed + Clock::now() - networkOutputStart);

	// Autosave
	if (advanceSimulation && gameRuntime.world.tickScheduler.currentTick % 40 == 0) {
		auto saveStart = Clock::now();
		SaveLevelFile();
		if (sampling)
			tickProfiler.Record(TickTask::Autosave, Clock::now() - saveStart);
	}

	// TODO: This is rather fragile!
	// Countdown
	if (shutdownTimer.load() > 1)
		shutdownTimer--;
	if (shutdownTimer.load() == 1)
		shutdownRequested.store(true);

#ifdef BETACRAFT_HEARTBEAT
	// TODO: This WILL cause issues!! FIND A BETTER WAY TO DO THIS!!!!!!
	if (betacraftHeartbeat.Enabled() && std::fmod(static_cast<double>(gameRuntime.world.tickScheduler.currentTick), GetTickRate())  < 1e-9) {
		BetacraftHeartbeatSnapshot snap;
		snap.maxPlayers = maximumPlayers;
		if (snap.maxPlayers < 0)
			snap.maxPlayers = 20;
		snap.onlineMode = config.GetAsBoolean("online-mode", false);
		for (const auto& session : players) {
			if (session->connState != ConnectionState::Playing)
				continue;
			++snap.onlinePlayers;
			if (!session->username.empty())
				snap.playerNames.push_back(session->username);
		}
		betacraftHeartbeat.UpdateSnapshot(snap);
	}
#endif
	if (sampling) {
		tickProfiler.Record(TickTask::TotalTick, Clock::now() - totalStart);
		++collectedSampleTicks;
		if (collectedSampleTicks >= requestedSampleTicks) {
			auto requester = sampleRequester.lock();
			const bool connected = sampleToConsole || (requester &&
			    std::any_of(players.begin(), players.end(), [&](const auto& player) {
				    return player == requester && player->connState == ConnectionState::Playing;
			    }));
			if (connected) {
				if (activeSampleMode == SampleMode::Health) {
					const std::array<std::pair<TickTask, const char*>, 11> tasks = {{
						{ TickTask::TotalTick, "Total Tick" },
						{ TickTask::Network, "Network" },
						{ TickTask::Autosave, "Autosave" },
						{ TickTask::OffTickTasks, "Off-Tick Tasks" },
						{ TickTask::MobSpawning, "Mob Spawning" },
						{ TickTask::ChunkLoading, "Chunk Loading" },
						{ TickTask::ChunkUnloading, "Chunk Unloading" },
						{ TickTask::BlockUpdates, "Block Updates" },
						{ TickTask::EntityTicks, "Entity Ticks" },
						{ TickTask::BlockEntityTicks, "Block Entity Ticks" },
						{ TickTask::Environment, "Environment" }
					}};
					for (const auto& [task, name] : tasks) {
						const double mean = std::chrono::duration<double, std::milli>(tickProfiler.Get(task)).count() /
						                    collectedSampleTicks;
						SendCommandFeedback(sampleToConsole ? nullptr : requester.get(),
						                    std::format("§7{}: {:.2f} ms/tick", name, mean));
					}
				} else {
					std::vector<std::pair<TickEntityKey, TickProfiler::EntitySample>> entities(
					    tickProfiler.EntitySamples().begin(), tickProfiler.EntitySamples().end());
					std::sort(entities.begin(), entities.end(), [](const auto& a, const auto& b) {
						return a.second.elapsed > b.second.elapsed;
					});
					SendCommandFeedback(sampleToConsole ? nullptr : requester.get(), "§7Entity tick cost (top 10):");
					for (size_t i = 0; i < std::min<size_t>(10, entities.size()); ++i) {
						const auto& [key, sample] = entities[i];
						SendCommandFeedback(sampleToConsole ? nullptr : requester.get(),
						                   std::format("§7dim {} entity {} type {}: {} ticks, {:.3f} ms",
						                            int(static_cast<int8_t>(key.dimension)), int32_t(key.id),
						                            int(static_cast<uint8_t>(key.type)), sample.count,
						                            std::chrono::duration<double, std::milli>(sample.elapsed).count()));
					}
				}
			}
			activeSampleMode = SampleMode::None;
			tickProfiler.Reset();
			sampleRequester.reset();
			sampleToConsole = false;
			requestedSampleTicks = collectedSampleTicks = 0;
		}
	}

	size_t eligibleCount = 0;
	bool namesChanged = false;

	for (const auto& session : players) {
		if (!session || !session->stream.IsConnected() || session->connState != ConnectionState::Playing ||
		    !session->entity)
			continue;
		if (eligibleCount >= playerNamesSnapshot.size() ||
		    playerNamesSnapshot[eligibleCount] != session->username)
			namesChanged = true;
		++eligibleCount;
	}

	if (eligibleCount != playerNamesSnapshot.size())
		namesChanged = true;

	if (namesChanged) {
		std::vector<std::string> names;
		names.reserve(eligibleCount);

		for (const auto& session : players) {
			if (session && session->stream.IsConnected() && session->connState == ConnectionState::Playing &&
			    session->entity)
				names.push_back(session->username);
		}

		playerNamesSnapshot = std::move(names);
		serverConsole.UpdatePlayerNames(playerNamesSnapshot);
	}
	// Flush logger output to file, if applicable
	Logger::Flush();
}

void Server::OnPlayerBlockBreak(PlayerSession& _session, WorldManager& _world) {
	// Actually break this block
	auto finishMiningWithTool = [&](ItemStack* _held, BlockType _block) {
		if (!_held)
			return;
		auto it = Items::toolBehavior.find(_held->id);
		if (it != Items::toolBehavior.end() && it->second.onBlockFinishMining)
			it->second.onBlockFinishMining(_held, _block);
	};

	auto blockId = _session.pendingBlockBreak->lastBlock;
	auto blockPos = _session.pendingBlockBreak->lastBlockPos;
	ItemStack* heldItem = _session.inventory.GetHeldItem();

	_session.pendingBlockBreak.reset();
	if (!Items::CanPlayerHarvest(heldItem, blockId)) {
		_world.SetBlock(blockPos, BLOCK_AIR);
		return;
	}

	if (_session.entity) {
		if (auto func = Blocks::blockBehaviors[blockId].onBlockDestroyedByPlayer) {
			func(_world, blockPos, *_session.entity);
		} else {
			Blocks::GenericBreak(_world, blockPos, *_session.entity);
		}
	}

	finishMiningWithTool(heldItem, blockId);

	// Send the particle packet
	Packet::WorldEvent pkt;
	pkt.eventType = PacketData::WorldEvent::BLOCK_BREAK;
	pkt.data = blockId;
	pkt.position = { blockPos.x, int8_t(blockPos.y), blockPos.z };
	_session.entityTracker->SendPacketToViewers(pkt, _session.entity->id);
}

bool Server::TryForceBreak(PlayerSession& _session, WorldManager& _world) {
	if (!_session.pendingBlockBreak.has_value())
		return false;

	if (_session.pendingBlockBreak->damage < 0.7f) {
		// We missed the client break, so we need to force it to break on the server side
		_session.pendingBlockBreak->clientBreakMissed = true;
		return false;
	}

	// Success!
	OnPlayerBlockBreak(_session, _world);
	return true;
}

void Server::UpdateBlockBreaking(PlayerSession& _session, WorldManager& _world) {
	if (!_session.pendingBlockBreak.has_value())
		return;

	auto miningConditions = [&]() -> std::pair<bool, bool> {
		bool inWater = _session.entity && _session.entity->HeadInWater();
		bool onGround = !_session.entity || _session.entity->onGround;
		return { inWater, onGround };
	};

	auto finishMiningWithTool = [&](ItemStack* _held, BlockType _block) {
		if (!_held)
			return;
		auto it = Items::toolBehavior.find(_held->id);
		if (it != Items::toolBehavior.end() && it->second.onBlockFinishMining)
			it->second.onBlockFinishMining(_held, _block);
	};

	auto [inWater, onGround] = miningConditions();
	auto blockId = _session.pendingBlockBreak->lastBlock;
	auto blockPos = _session.pendingBlockBreak->lastBlockPos;
	ItemStack* heldItem = _session.inventory.GetHeldItem();

	auto damagePerTick = Items::BlockDamagePerTick(heldItem, blockId, inWater, onGround);

	// Client breaks immediately when blockStrength >= 1
	if (damagePerTick >= 1.0f) {
		if (_session.entity) {
			if (auto func = Blocks::blockBehaviors[blockId].onBlockDestroyedByPlayer) {
				func(_world, blockPos, *_session.entity);
			} else {
				Blocks::BreakAndDropBlock(_world, blockPos);
			}
		}
		finishMiningWithTool(heldItem, blockId);
		_session.pendingBlockBreak.reset();
		return;
	}

	// Dont break if our damage per tick is too low
	if (damagePerTick <= 0.0f) {
		_session.pendingBlockBreak.reset();
		return;
	}

	// See if we can actually break this block
	if (_session.pendingBlockBreak->damage < 1.0f || !_session.pendingBlockBreak->clientBreakMissed) {
		_session.pendingBlockBreak->damage = std::min(1.0f, _session.pendingBlockBreak->damage + damagePerTick);
		return;
	}

	OnPlayerBlockBreak(_session, _world);
}

void Server::ProcessSleeping(Dimension _dimension) {
	// How long (in ticks) a player has to lie in bed before they're "fully asleep"
	static constexpr int SLEEP_TIMER_TARGET = 100;

	WorldManager* world = GetWorldForDimension(_dimension);

	int playingCount = 0;
	int fullyAsleepCount = 0;
	std::vector<std::shared_ptr<PlayerSession>> sleepingSessions;

	for (auto& session : players) {
		if (!session || session->connState != ConnectionState::Playing || session->dimension != _dimension)
			continue;
		if (!session->entity)
			continue;
		playingCount++;

		if (!session->entity->isSleeping)
			continue;

		// Check if our bed is still there
		// and check if it's really still night (in case someone used commands)
		if (world->GetBlockId(session->entity->bedPosition) != BLOCK_BED || !world->IsNight()) {
			session->entity->WakeUp(/*_confirmSpawn=*/false);
			continue;
		}

		sleepingSessions.push_back(session);
		if (session->entity->ticksInBed >= SLEEP_TIMER_TARGET)
			fullyAsleepCount++;
	}

	// Nobody's here, or not everybody is fully asleep yet
	if (playingCount == 0 || fullyAsleepCount < playingCount)
		return;

	bool spawnedNightmare = false;
	if (Gamerule::Get<bool>(Gamerule::Id::Nightmares)) {
		for (auto& session : sleepingSessions)
			spawnedNightmare |= Blocks::TrySpawnNightmare(*world, *session->entity);
	}

	if (spawnedNightmare)
		return;

	// Fast-forward to the next morning.
	world->elapsedTicks = ((world->elapsedTicks / DAY_LENGTH) + 1) * DAY_LENGTH;
	world->weatherSystem.ClearWeather();

	// Wake everyone up.
	for (auto& session : sleepingSessions)
		session->entity->WakeUp(/*_confirmSpawn=*/true);
}

std::vector<std::shared_ptr<PlayerSession>> Server::DisconnectClients() {
	// Mark clients who have timed out for removal
	auto now = std::chrono::steady_clock::now();
	for (auto& session : players) {
		if (session->connState == ConnectionState::Playing) {
			auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - session->lastPacketTime).count();
			if (elapsed > timeoutSeconds) {
				Logger::Info << "Player " << session->username << " timed out\n";
				connStateManager.DisconnectPlayer(*session, "Connection timed out.", *this);
			}
		} else {
			// Kill stuck handshakers
			auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - session->lastPacketTime).count();
			if (elapsed > timeoutSeconds) {
				session->stream.SetConnected(false);
				Logger::Info << "Disconnected dataless stream. (Most likely a prober!)\n";
			}
		}
	}

	// Force disconnect players that quit
	std::vector<std::shared_ptr<PlayerSession>> removed;
	players.erase(std::remove_if(players.begin(), players.end(),
	                             [&removed](const auto& session) {
		                             if (session->stream.IsConnected())
			                             return false;
		                             removed.push_back(session);
		                             return true;
	                             }),
	              players.end());

	for (auto& session : removed) {
		if (session->entity) {
			Logger::Info << "Disconnected client " << session->username
			                    << " with entity id " << session->entity->id << "\n";
			SendGlobalChatMessage("§e" + session->username + " left the game.");

			const bp_player_leave_event event{ &session->apiPlayer };
			addonManager.Broadcast(&bp_addon_events::playerLeave, event);

			if (session->entity->entityManager)
				session->entity->entityManager->RemoveEntity(session->entity->id);
		}
		IndexRemoveSession(*session);
		chunkSender.Remove(*session);
	}
	return removed;
};

void Server::ProcessIncoming(PlayerSession& _session) {
	WorldManager& sessionWorld = _session.dimension == Dimension::Nether ? gameRuntime.worldHell : gameRuntime.world;

	bool recvPacket = false;

	while (_session.stream.HasData()) {
		size_t packetMark = _session.stream.Mark();

		PacketId packetId = _session.stream.Read<PacketId>();

		if (_session.stream.CheckAndClearShortRead()) {
			// Not even the ID has fully arrived yet.
			_session.stream.Rollback(packetMark);
			break;
		}

		if (!PacketDispatcher::Dispatch(packetId, _session, sessionWorld, *this))
			return; // session is dead, or sent an unknown packet

		if (_session.stream.CheckAndClearShortRead()) {
			// The packet wasn't full.
			_session.stream.Rollback(packetMark);
			break;
		}

		recvPacket = true;
	}

	// Update our last packet time for the timeout code
	if (recvPacket)
		_session.lastPacketTime = std::chrono::steady_clock::now();
}
