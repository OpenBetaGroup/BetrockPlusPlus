/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#include "../packet/packet_utils.h"
#include "../server.h"
#include "addon/addon_impl.h"
#include "username.h"
#include "version.h"
#include "teleporter.h"

void PlayerConnStateManager::HandleConnectionState(PlayerSession& _session, Server& _server) {
	switch (_session.connState) {
	case ConnectionState::Handshaking:
		HandleHandshake(_session, _server);
		break;
	case ConnectionState::LoggingIn:
		HandleLogin(_session, _server);
		break;
	case ConnectionState::VerifyingUsername:
		HandleVerifyingUsername(_session, _server);
		break;
	case ConnectionState::WaitingForSpawnChunks:
		WaitForSpawnChunks(_session, _server);
		break;
	case ConnectionState::Playing: {
		WorldManager& sessionWorld = _session.dimension == Dimension::Nether ? _server.gameRuntime.worldHell
		                                                                     : _server.gameRuntime.world;
		_server.chunkSender.Enqueue(_session, sessionWorld, 16);
		_server.chunkSender.Flush(_session);
		if (_server.IsTickFrozen() || sessionWorld.elapsedTicks % 20 == 0) {
			// Update the server time so client's don't desync
			Packet::SetTime time;
			time.time = sessionWorld.elapsedTicks;
			time.Serialize(_session.stream);
		}
		break;
	}
	}
}

void PlayerConnStateManager::HandleHandshake(PlayerSession& _session, Server& _server) {
	if (!_session.stream.HasData())
		return;
	PacketId packetId = _session.stream.Read<PacketId>();

	if (_session.stream.CheckAndClearShortRead())
		return;
	if (packetId != PacketId::PreLogin)
		return;

	Packet::PreLogin incoming;
	incoming.Deserialize(_session.stream);
	if (_session.stream.CheckAndClearShortRead()) {
		return;
	}

	// NOTE: Ornite hyjacks the PreLogin packet to report which mods it has,
	// so we shouldn't use the PreLogin username to identification.
	// This works on Vanilla Beta 1.7.3, so we should replicate this.
	if (incoming.usernameServerId.starts_with("^@"))
		Logger::Info << "Modified client! Provided mods: " << incoming.usernameServerId << '\n';

	// Authentication
	_session.serverId = "-";
#ifdef ONLINE_MODE_AUTHENTICATION
	if (_server.auth.onlineMode)
		_session.serverId = _server.auth.GenerateAuthHash();
#endif

	Packet::PreLogin response;
	response.usernameServerId = _session.serverId;
	response.Serialize(_session.stream);

	_session.connState = ConnectionState::LoggingIn;
}

void PlayerConnStateManager::HandleLogin(PlayerSession& _session, Server& _server) {
	if (!_session.stream.HasData())
		return;

	PacketId packetId = _session.stream.Read<PacketId>();
	if (_session.stream.CheckAndClearShortRead())
		return;
	if (packetId != PacketId::Login)
		return;

	Packet::Login incoming;
	incoming.Deserialize(_session.stream);
	if (_session.stream.CheckAndClearShortRead()) {
		return;
	}
	// Check if the username contains any invalid characters
	if (!IsValidUsername(incoming.username, _server.config.GetAsBoolean("allow-floodgate-prefix"))) {
		DisconnectPlayer(_session, "Invalid username!", _server, false);
		return;
	}
	_session.username = incoming.username;

	// User is banned, reject
	if (ContainsUsername(_server.bannedUsernames, incoming.username)) {
		DisconnectPlayer(_session, "You are banned from this server!", _server, false);
		return;
	}

	// User's IP is banned, reject
	if (!_session.ipAddress.empty() &&
	    std::find(_server.bannedIps.begin(), _server.bannedIps.end(), _session.ipAddress) != _server.bannedIps.end()) {
		DisconnectPlayer(_session, "Your IP address is banned from this server!", _server, false);
		return;
	}

	// User isn't whitelisted, reject
	if (_server.useWhitelist && !ContainsUsername(_server.whitelistedUsernames, incoming.username)) {
		DisconnectPlayer(_session, "You're not whitelisted!", _server, false);
		return;
	}

	// Reject duplicate usernames
	const auto& players = _server.GetPlayers();

	// NOTE: We can't just use GetSessionByUsername,
	// because the session with this username already exists when you're connecting to the server
	auto it = std::find_if(players.begin(), players.end(),
	                       [&incoming, &_session](const std::shared_ptr<PlayerSession>& _player) {
		                       return _player && _player.get() != &_session && UsernameEquals(_player->username, incoming.username);
	                       });

	if (it != players.end()) {
		DisconnectPlayer(_session, "Player with same username is already on server!", _server, false);
		return;
	}

	Logger::Info << "Player " << _session.username << " is logging in.\n";

#ifdef ONLINE_MODE_AUTHENTICATION
	if (_server.auth.onlineMode) {
		_session.pendingAuthFuture = _server.auth.IsRegisteredUsernameAsync(_session.serverId, _session.username);
		_session.authStartTime = std::chrono::steady_clock::now();
		_session.connState = ConnectionState::VerifyingUsername;
		return;
	}
#endif

	FinishLogin(_session, _server);
}

void PlayerConnStateManager::HandleVerifyingUsername(PlayerSession& _session, Server& _server) {
#ifdef ONLINE_MODE_AUTHENTICATION
	using namespace std::chrono_literals;
	constexpr auto K_AUTH_TIMEOUT = 20s;

	if (!_session.pendingAuthFuture.valid()) {
		DisconnectPlayer(_session, "Failed to verify username!", _server, false);
		return;
	}

	if (_session.pendingAuthFuture.wait_for(0s) != std::future_status::ready) {
		if (std::chrono::steady_clock::now() - _session.authStartTime > K_AUTH_TIMEOUT)
			DisconnectPlayer(_session, "Login verification timed out!", _server, false);
		return; // Not ready yet - check again next tick.
	}

	const bool verified = _session.pendingAuthFuture.get();
	if (!verified) {
		DisconnectPlayer(_session, "Failed to verify username!", _server, false);
		return;
	}
#endif
	FinishLogin(_session, _server);
}

void PlayerConnStateManager::FinishLogin(PlayerSession& _session, Server& _server) {
	// Initialize our entity first as the player session depends on it
	if (!_session.entity)
		_session.entity = std::make_shared<EntityMPPlayer>(std::string(_session.username.begin(), _session.username.end()));

	// Load player data before building the Login response so we know which dimension they're in
	auto playerNbt = _server.gameRuntime.saveManager.GetPlayerNbt(
	    std::string(_session.username.begin(), _session.username.end()));
	_session.LoadPlayerNbt(playerNbt);

	// Get the right world pointer
	WorldManager& sessionWorld = _session.dimension == Dimension::Nether ? _server.gameRuntime.worldHell
	                                                                     : _server.gameRuntime.world;

	_session.entity->session = &_session;
	_session.entity->id = sessionWorld.entityManager.GetNextEntityId();
	_session.entity->dim = _session.dimension == Dimension::Nether ? Dimension::Nether : Dimension::Overworld;

	{
		const bp_player_join_event event{ &_session.apiPlayer };
		_server.GetAddonManager().Broadcast(&bp_addon_events::playerJoin, event);
	}

	Packet::Login response;
	response.entityIdProtocolVersion = _session.entity->id;
	response.username = _session.username;
	response.worldSeed = _server.gameRuntime.world.seed;
	response.dimension = Dimension(_session.dimension);
	response.Serialize(_session.stream);

	Packet::SetSpawnPosition spawn;
	spawn.position = sessionWorld.spawnPoint;
	spawn.Serialize(_session.stream);

	Packet::SetHealth health;
	health.health = static_cast<int16_t>(_session.entity->health);
	health.Serialize(_session.stream);

	Packet::SetTime time;
	time.time = sessionWorld.elapsedTicks;
	time.Serialize(_session.stream);

	// Get a fresh respawn point
	auto respawnPoint = sessionWorld.GetSpawnPoint(true);

	// If our session position is the default then overwrite it
	if (_session.position.pos == Vec3{ -1, -1000000, -1 }) {
		_session.position.pos = { float(respawnPoint.x) + 0.5, float(respawnPoint.y), float(respawnPoint.z) + 0.5 };
	}

	// Small nudge
	_session.position.pos.y += 0.01;

	// Log that we logged in!
	Logger::Info << "Player " << _session.username << " logged in with entity ID " << _session.entity->id
	                    << " at (" << _session.position.pos.x << ", " << _session.position.pos.y << ", "
	                    << _session.position.pos.z << ")\n";

	// Update our last trusted position and rotation
	_session.entity->position = _session.position.pos;
	_session.entity->rotationYaw = _session.rotation.x;
	_session.entity->rotationPitch = _session.rotation.y;
	_session.pendingTeleport = _session.position.pos;
	_session.entity->RebuildCollider();

	// Let everyone else know we logged in
	_server.SendGlobalChatMessage("§e" + _session.username + " joined the game.");

	// Send our inventory
	PacketUtilities::SendInventory(_session, 0, _session.inventory);

	// Snapshot current contents
	_session.inventoryInteraction.InitSnapshot();

	_session.connState = ConnectionState::WaitingForSpawnChunks;
}

void PlayerConnStateManager::DisconnectPlayer(PlayerSession& _session, const std::string& _reason, Server& _server,
                                              bool _doSave) {
	// Force close inventory
	if (_session.entity)
		_session.inventoryInteraction.OnInteractionClosed(*_session.entity);

	// Force close our window
	if (_session.activeInteraction && _session.entity)
		_session.activeInteraction->OnInteractionClosed(*_session.entity);

	// Save our player data
	if (_doSave) {
		if (_session.entity)
			_session.entity->UnmountEntity();
		_server.SavePlayer(_session.username);
	}

	// Send disconnect reason to the leaving player
	Packet::Disconnect kick;
	kick.reason = _reason;
	kick.Serialize(_session.stream);
	_session.stream.FlushWriteBuffer();
	_session.stream.SetConnected(false);
	_session.server = nullptr;
	Logger::Info << "Player " << _session.username << " disconnected: " << _reason << "\n";
}

static constexpr int PORTAL_SEARCH_CHUNK_RADIUS = 8; // 128 blocks
static constexpr int PORTAL_BUILD_CHUNK_RADIUS = 2;

static bool PortalAreaReady(WorldManager& _world, const Vec3& _pos) {
	int cx = MathHelper::FloorDouble(_pos.x) >> 4;
	int cz = MathHelper::FloorDouble(_pos.z) >> 4;

	for (int dx = -PORTAL_SEARCH_CHUNK_RADIUS; dx <= PORTAL_SEARCH_CHUNK_RADIUS; dx++) {
		for (int dz = -PORTAL_SEARCH_CHUNK_RADIUS; dz <= PORTAL_SEARCH_CHUNK_RADIUS; dz++) {
			Chunk* c = _world.GetChunkRaw({ cx + dx, cz + dz });
			if (!_world.IsChunkValid(c))
				return false; // not generated/loaded yet

			bool inBuildArea = std::abs(dx) <= PORTAL_BUILD_CHUNK_RADIUS && std::abs(dz) <= PORTAL_BUILD_CHUNK_RADIUS;
			if (inBuildArea && !c->isTerrainPopulated)
				return false; // population could overwrite a portal
		}
	}
	return true;
}

void PlayerConnStateManager::WaitForSpawnChunks(PlayerSession& _session, Server& _server) {
	WorldManager& sessionWorld = _session.dimension == Dimension::Nether ? _server.gameRuntime.worldHell
	                                                                     : _server.gameRuntime.world;
	_session.position.viewDistanceOverride = _session.pendingPortalPlacement
	                                            ? PORTAL_SEARCH_CHUNK_RADIUS + 1
	                                            : (_session.portalAreaLoaded ? 0 : 3);
	_server.chunkSender.Enqueue(_session, sessionWorld, _server.flushChunkCount);
	_server.chunkSender.Flush(_session);

	if (_session.pendingPortalPlacement) {
		if (!PortalAreaReady(sessionWorld, _session.position.pos))
			return;

		Teleporter::PlaceAtValidTransferSpot(sessionWorld, *_session.entity);
		_session.position.pos = _session.entity->position;
		_session.pendingTeleport = _session.position.pos;
		_session.pendingPortalPlacement = false;
		_session.portalAreaLoaded = true;
	}

	// Spawn chunk radius; 3 chunks in each direction
	int spawnChunkX = int(std::floor(_session.position.pos.x)) >> 4;
	int spawnChunkZ = int(std::floor(_session.position.pos.z)) >> 4;

	int radius = std::min<int>(3, sessionWorld.GetViewRadius());

	int totalSpawnChunks = ((radius * 2) + 1) * ((radius * 2) + 1);
	int loadedChunks = 0;

	for (int dx = -radius; dx <= radius; dx++) {
		for (int dz = -radius; dz <= radius; dz++) {
			Int32_2 p{ spawnChunkX + dx, spawnChunkZ + dz };
			if (_session.flushedChunks.contains(p))
				loadedChunks++;
		}
	}

	int percent = static_cast<int>(std::ceil((float(loadedChunks) / float(totalSpawnChunks)) * 100.0f));

	if (loadedChunks % (totalSpawnChunks / 4) == 0)
		Logger::Info << "Spawn chunks: " << percent << "%\n";

	if (loadedChunks < totalSpawnChunks)
		return;

	Logger::Info << "Spawn chunks sent.\n";

	Packet::PlayerPositionAndRotation pos;
	pos.position = { _session.position.pos.x, _session.position.pos.y + PLAYER_EYE_HEIGHT, _session.position.pos.z };
	pos.cameraY = _session.position.pos.y;
	pos.rotation = _session.rotation;
	pos.onGround = false;
	pos.Serialize(_session.stream);

	// Update our last trusted position
	_session.entity->position = _session.position.pos;
	_session.pendingTeleport = _session.position.pos;
	_session.entity->RebuildCollider();

	// Set view distance to server default
	_session.position.viewDistanceOverride = 0;
	_session.portalAreaLoaded = false;
	_session.pendingPortalPlacement = false;

	// Send weather
	PacketUtilities::SendWeatherToPlayer(_session, sessionWorld.weatherSystem.GetWeatherState());

	// Send time
	Packet::SetTime tPkt;
	tPkt.time = sessionWorld.elapsedTicks;
	tPkt.Serialize(_session.stream);

	Logger::Info << "Client connected\n";
	_session.connState = ConnectionState::Playing;

	// Register our entity with the world
	if (!_session.entityRegistered)
		sessionWorld.entityManager.AddEntity(_session.entity, _session.entity->id);
	_session.entityRegistered = true;

	// Give our player session a pointer to the entity tracker
	_session.entityTracker = _session.dimension == Dimension::Overworld ? &_server.overworldEntityTracker
	                                                                    : &_server.hellEntityTracker;

	// Welcome message
	Packet::ChatMessage welcomeMsg;
	welcomeMsg.message = std::string("§eThis Server runs on ") + std::string(PROJECT_FULL_VERSION_LABEL);
	welcomeMsg.Serialize(_session.stream);
}