/*
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "addon_impl.h"
#include "addon.h"
#include "base_structs.h"
#include "blocks.h"
#include "logger.h"
#include "server.h"
#include "version.h"

static constexpr Addon* InternalGetAddon(const bp_api* _api) {
	return static_cast<Addon*>(_api->internal);
}

void LogInfo(const bp_api* _api, const char* _message) {
	Addon* addon = InternalGetAddon(_api);
	Logger::Info << "[" << addon->info.name << "] " << _message << "\n";
}

void LogWarning(const bp_api* _api, const char* _message) {
	Addon* addon = InternalGetAddon(_api);
	Logger::Warning << "[" << addon->info.name << "] " << _message << "\n";
}

void LogError(const bp_api* _api, const char* _message) {
	Addon* addon = InternalGetAddon(_api);
	Logger::Error << "[" << addon->info.name << "] " << _message << "\n";
}

int GetPlayerCount(const bp_api* _api) {
	Addon* addon = InternalGetAddon(_api);
	return addon->server->GetPlayers().size();
}

bp_player* GetPlayerAt(const bp_api* _api, int _index) {
	Addon* addon = InternalGetAddon(_api);
	const auto& players = addon->server->GetPlayers();

	if (_index < 0 || static_cast<size_t>(_index) >= players.size())
		return nullptr;

	const auto& player = players[size_t(_index)];
	if (!player)
		return nullptr;

	return &player->apiPlayer;
}

void ServerStop(const bp_api* _api, float _timeout) {
	if (!_api || !_api->internal)
		return;
	Addon* addon = InternalGetAddon(_api);
	if (!addon->server)
		return;
	addon->server->StopTimeout(_timeout);
}

double ServerGetTpsAverage(const bp_api* _api) {
	if (!_api || !_api->internal)
		return 0.0;
	Addon* addon = InternalGetAddon(_api);
	if (!addon->server)
		return 0.0;
	const double tps = addon->server->averageTickPerSecond.load();
	return tps > 0.0 ? tps : addon->server->GetTickRate();
}

double ServerGetMsptAverage(const bp_api* _api) {
	if (!_api || !_api->internal)
		return 0.0;
	Addon* addon = InternalGetAddon(_api);
	if (!addon->server)
		return 0.0;
	return addon->server->averageTickMs.load();
}

const char* ServerGetVersion() {
	return PROJECT_VERSION_FULL_STRING.data();
}

void PlayerSendMessage(bp_player* _player, const char* _message) {
	Packet::ChatMessage pak;
	pak.message = std::string(_message);
	pak.Serialize(_player->session->stream);
}

void PlayerKick(const bp_api* _api, bp_player* _player, const char* _reason) {
	Addon* addon = InternalGetAddon(_api);
	addon->server->DisconnectPlayer(_reason, *_player->session);
}

const char* PlayerGetUsername(bp_player* _player) {
	return _player->session->username.c_str();
}

bp_entity* PlayerGetEntity(bp_player* _player) {
	// Peak C++
	return &_player->session->entity->apiEntity;
}

bp_vec3 EntityGetPosition(bp_entity* _entity) {
	return { _entity->entity->position.x, _entity->entity->position.y, _entity->entity->position.z };
}

void EntitySetPosition(bp_entity* _entity, bp_vec3 _pos) {
	_entity->entity->Teleport({ _pos.x, _pos.y, _pos.z });
}

bp_world* EntityGetWorld(bp_entity* _entity) {
	return &_entity->entity->world->apiWorld;
}

bp_block GetBlock(bp_world* _world, bp_block_pos _pos) {
	Int3 blockPos{ _pos.x, _pos.y, _pos.z };
	auto id = _world->manager->GetBlockId(blockPos);
	auto meta = _world->manager->GetMetadata(blockPos);
	return bp_block{ .id = id, .meta = meta };
}

void SetBlock(bp_world* _world, bp_block_pos _pos, bp_block _block) {
	Int3 blockPos{ _pos.x, _pos.y, _pos.z };
	_world->manager->SetBlock(blockPos, Block{ .type = static_cast<BlockType>(_block.id), .data = _block.meta });
}

void SendBlockUpdate(bp_world* _world, bp_block_pos _pos, bp_block _block) {
	Int3 blockPos{ _pos.x, _pos.y, _pos.z };
	if (!_world->manager->onBlockUpdate)
		return;
	auto* chunk = _world->manager->GetChunkRaw({ _pos.x >> 4, _pos.z >> 4 });
	if (!chunk)
		return;
	Int3 local{ _pos.x & 15, _pos.y, _pos.z & 15 };
	_world->manager->onBlockUpdate(PendingBlock{ .block{ .type = static_cast<BlockType>(_block.id),
	                                                     .data = _block.meta },
	                                             .blockPos{ _pos.x, _pos.y, _pos.z },
	                                             .light{ chunk->GetBlockLight(local), chunk->GetSkyLight(local) } },
	                               chunk->cpos);
}

void* GetPlayerData(const bp_api* _api, bp_player* _player) {
	Addon* addon = InternalGetAddon(_api);
	auto it = _player->addonData.find(addon);
	if (it == _player->addonData.end())
		return nullptr;

	return it->second;
}

void SetPlayerData(const bp_api* _api, bp_player* _player, void* _data) {
	_player->addonData[InternalGetAddon(_api)] = _data;
}

bool ExecuteAddonCommand(const bp_api* _api, bp_player* _sender, const char* _command) noexcept {
	try {
		if (!_api || !_api->internal || !_command)
			return false;

		Addon* addon = InternalGetAddon(_api);
		if (!addon || !addon->server)
			return false;
		Server* server = addon->server;
		if (!server->CanExecuteCommands())
			return false;

		std::shared_ptr<PlayerSession> sender;
		if (_sender) {
			for (const auto& session : server->GetPlayers()) {
				if (session && &session->apiPlayer == _sender) {
					sender = session;
					break;
				}
			}
			if (!sender)
				return false;
		}

		return server->ExecuteCommand(_command, sender.get());
	} catch (...) {
		return false;
	}
}

bp_api MakeAddonAPI() {
	return bp_api{
		.version = ADDON_API_VERSION,
		.internal = nullptr, // Set by the addon manager
		.log = { .info = LogInfo, .warning = LogWarning, .error = LogError },
		.server = { .getPlayerCount = GetPlayerCount, .getPlayerAt = GetPlayerAt, .stop = ServerStop, .getMsptAverage = ServerGetMsptAverage, .getTpsAverage = ServerGetTpsAverage, .getVersion = ServerGetVersion },
		.player = { .sendMessage = PlayerSendMessage, .kick = PlayerKick, .getUsername = PlayerGetUsername, .getEntity = PlayerGetEntity, },
		.entity = { .getPosition = EntityGetPosition, .setPosition = EntitySetPosition, .getWorld = EntityGetWorld },
		.world = { .setBlock = SetBlock, .getBlock = GetBlock, .sendBlockUpdate = SendBlockUpdate },
		.data = { .setPlayer = SetPlayerData, .getPlayer = GetPlayerData },
		.commands = { .execute = ExecuteAddonCommand }
	};
}