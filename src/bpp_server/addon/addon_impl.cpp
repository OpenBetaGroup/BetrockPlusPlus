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
#include "commands/command.h"
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

	return reinterpret_cast<bp_player*>(player.get());
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
	auto player = reinterpret_cast<PlayerSession*>(_player);

	Packet::ChatMessage pak;
	pak.message = std::string(_message);
	pak.Serialize(player->stream);
}

void PlayerKick(const bp_api* _api, bp_player* _player, const char* _reason) {
	auto player = reinterpret_cast<PlayerSession*>(_player);
	Addon* addon = InternalGetAddon(_api);
	addon->server->DisconnectPlayer(_reason, *player);
}

const char* PlayerGetUsername(bp_player* _player) {
	auto player = reinterpret_cast<PlayerSession*>(_player);
	return player->username.c_str();
}

bp_entity* PlayerGetEntity(bp_player* _player) {
	auto player = reinterpret_cast<PlayerSession*>(_player);
	return reinterpret_cast<bp_entity*>(player->entity.get());
}

bp_vec3 EntityGetPosition(bp_entity* _entity) {
	auto entity = reinterpret_cast<Entity*>(_entity);
	return { entity->position.x, entity->position.y, entity->position.z };
}

void EntitySetPosition(bp_entity* _entity, bp_vec3 _pos) {
	auto entity = reinterpret_cast<Entity*>(_entity);
	entity->Teleport({ _pos.x, _pos.y, _pos.z });
}

bp_world* EntityGetWorld(bp_entity* _entity) {
	auto entity = reinterpret_cast<Entity*>(_entity);
	return reinterpret_cast<bp_world*>(entity->world);
}

bp_player* EntityToPlayer(bp_entity* _entity) {
	// This code is just perfect
	auto entity = reinterpret_cast<Entity*>(_entity);
	if (auto player = dynamic_cast<EntityMPPlayer*>(entity)) {
		return reinterpret_cast<bp_player*>(player);
	}
	return nullptr;
}

bp_block GetBlock(bp_world* _world, bp_block_pos _pos) {
	auto world = reinterpret_cast<WorldManager*>(_world);
	Int3 blockPos{ _pos.x, _pos.y, _pos.z };
	auto id = world->GetBlockId(blockPos);
	auto meta = world->GetMetadata(blockPos);
	return bp_block{ .id = id, .meta = meta };
}

void SetBlock(bp_world* _world, bp_block_pos _pos, bp_block _block) {
	auto world = reinterpret_cast<WorldManager*>(_world);
	Int3 blockPos{ _pos.x, _pos.y, _pos.z };
	world->SetBlock(blockPos, Block{ .type = static_cast<BlockType>(_block.id), .data = _block.meta });
}

void SendBlockUpdate(bp_world* _world, bp_block_pos _pos, bp_block _block) {
	auto world = reinterpret_cast<WorldManager*>(_world);
	if (!world->onBlockUpdate)
		return;
	auto* chunk = world->GetChunkRaw({ _pos.x >> 4, _pos.z >> 4 });
	if (!chunk)
		return;
	Int3 local{ _pos.x & 15, _pos.y, _pos.z & 15 };
	world->onBlockUpdate(PendingBlock{ .block{ .type = static_cast<BlockType>(_block.id), .data = _block.meta },
	                                   .blockPos{ _pos.x, _pos.y, _pos.z },
	                                   .light{ chunk->GetBlockLight(local), chunk->GetSkyLight(local) } },
	                     chunk->cpos);
}

bool ExecuteCommand(const bp_api* _api, bp_player* _sender, const char* _command) noexcept {
	try {
		if (!_api || !_api->internal || !_command)
			return false;

		Addon* addon = InternalGetAddon(_api);
		if (!addon || !addon->server)
			return false;
		Server* server = addon->server;
		if (!server->CanExecuteCommands())
			return false;

		auto sender = reinterpret_cast<PlayerSession*>(_sender);
		return server->ExecuteCommand(_command, sender);
	} catch (...) {
		return false;
	}
}

bp_cmd_node* MakeCommandNode(const char* _name, bp_cmd_node_type _type) {
	auto node = new bp_cmd_node();
	node->name = _name;
	// The ordering matches, we can cast directly
	node->type = static_cast<strategos::NodeType>(_type);
	return node;
}

bp_cmd_node* MakeIntegerCmdNode(const char* _name, int _min, int _max) {
	auto node = MakeCommandNode(_name, BP_CMD_INTEGER);
	node->settings = strategos::IntSettings{ _min, _max };
	return node;
}

bp_cmd_node* MakeFloatCmdNode(const char* _name, float _min, float _max) {
	auto node = MakeCommandNode(_name, BP_CMD_FLOAT);
	node->settings = strategos::FloatSettings{ _min, _max };
	return node;
}

bp_cmd_node* MakeEnumCmdNode(const char* _name, const char* _values[]) {
	auto node = MakeCommandNode(_name, BP_CMD_ENUM);
	std::vector<std::string> valuesVec;
	if (_values != nullptr) {
		for (size_t i = 0; _values[i] != nullptr; ++i) {
			valuesVec.emplace_back(_values[i]);
		}
	}
	node->settings = strategos::EnumSettings{ std::move(valuesVec) };
	return node;
}

void AddCmdNodeChild(bp_cmd_node* _parent, bp_cmd_node* _child) {
	_parent->children.push_back(_child);
}

void SetCmdNodeAction(const bp_api* _api, bp_cmd_node* _node, bp_cmd_action_fn _action) {
	_node->action = [_action, _api](const strategos::CmdNode& _cmdNode, void* _userData) {
		const bp_cmd_action_ctx context{ _cmdNode, CmdCtx(_userData) };
		_action(_api, &context);
	};
}

void SendCmdActionFeedback(const bp_cmd_action_ctx* _ctx, const char* _message) {
	SendFeedback(_ctx->cmdCtx, _message);
}

bp_entity* GetCmdActionExecutor(const bp_cmd_action_ctx* _ctx) {
	return reinterpret_cast<bp_entity*>(_ctx->cmdCtx.executionEntity);
}

bool GetCmdStringArg(const bp_cmd_action_ctx* _ctx, const char* _name, const char** _out) {
	if (auto value = _ctx->cmdNode.get_arg<std::string>(_name)) {
		*_out = value->c_str();
		return true;
	}
	return false;
}

bool GetCmdIntegerArg(const bp_cmd_action_ctx* _ctx, const char* _name, int* _out) {
	if (auto value = _ctx->cmdNode.get_arg<int>(_name)) {
		*_out = *value;
		return true;
	}
	return false;
}

bool GetCmdFloatArg(const bp_cmd_action_ctx* _ctx, const char* _name, float* _out) {
	if (auto value = _ctx->cmdNode.get_arg<float>(_name)) {
		*_out = *value;
		return true;
	}
	return false;
}

bool GetCmdBooleanArg(const bp_cmd_action_ctx* _ctx, const char* _name, bool* _out) {
	if (auto value = _ctx->cmdNode.get_arg<bool>(_name)) {
		*_out = *value;
		return true;
	}
	return false;
}

bool GetCmdEnumArg(const bp_cmd_action_ctx* _ctx, const char* _name, const char** _out) {
	if (auto value = _ctx->cmdNode.get_arg<std::string>(_name)) {
		*_out = value->c_str();
		return true;
	}
	return false;
}

bool GetCmdVec3Arg(const bp_cmd_action_ctx* _ctx, const char* _name, bp_cmd_vec3* _out) {
	if (auto value = _ctx->cmdNode.get_arg<strategos::Vec3>(_name)) {
		_out->pos[0] = value->pos[0];
		_out->pos[1] = value->pos[1];
		_out->pos[2] = value->pos[2];
		_out->relativeAxes = value->relative_axes;
		return true;
	}
	return false;
}

strategos::Node TranslateCommandTree(const bp_cmd_node* _root) {
	strategos::Node newRoot;
	// Copy everything except the extra children vector
	// NOLINTNEXTLINE(cppcoreguidelines-slicing) // Suppress object slicing warning
	static_cast<strategos::AbstractNode&>(newRoot) = *_root;

	for (auto* child : _root->children) {
		newRoot.then(TranslateCommandTree(child));
	}

	delete _root;
	return newRoot;
}

void RegisterCommand(const bp_api* _api, bp_cmd_node* _root) {
	Addon* addon = InternalGetAddon(_api);
	addon->server->GetCommandManager().Dispatcher().add_command(TranslateCommandTree(_root));
}

bp_api MakeAddonAPI() {
	return bp_api{
		.version = ADDON_API_VERSION,
		.internal = nullptr, // Set by the addon manager
		.log = { .info = LogInfo, .warning = LogWarning, .error = LogError },
		.server = { .getPlayerCount = GetPlayerCount, .getPlayerAt = GetPlayerAt, .stop = ServerStop, .getMsptAverage = ServerGetMsptAverage, .getTpsAverage = ServerGetTpsAverage, .getVersion = ServerGetVersion },
		.player = { .sendMessage = PlayerSendMessage, .kick = PlayerKick, .getUsername = PlayerGetUsername, .getEntity = PlayerGetEntity, },
		.entity = { .getPosition = EntityGetPosition, .setPosition = EntitySetPosition, .getWorld = EntityGetWorld, .toPlayer = EntityToPlayer },
		.world = { .setBlock = SetBlock, .getBlock = GetBlock, .sendBlockUpdate = SendBlockUpdate },
		.cmd = { .makeNode = MakeCommandNode,
			.makeIntegerEx = MakeIntegerCmdNode,
			.makeFloatEx = MakeFloatCmdNode,
			.makeEnum = MakeEnumCmdNode,
			.addChild = AddCmdNodeChild,
			.sendActionFeedback = SendCmdActionFeedback,
			.getActionExecutor = GetCmdActionExecutor,
			.getStringArg = GetCmdStringArg,
			.getIntegerArg = GetCmdIntegerArg,
			.getFloatArg = GetCmdFloatArg,
			.getBooleanArg = GetCmdBooleanArg,
			.getEnumArg = GetCmdEnumArg,
			.getVec3Arg = GetCmdVec3Arg,
			.setAction = SetCmdNodeAction,
			.registerCommand = RegisterCommand,
			.execute = ExecuteCommand }
	};
}