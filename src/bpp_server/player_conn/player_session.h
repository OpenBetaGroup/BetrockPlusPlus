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
#include "addon/addon_impl.h"
#include "dimensions.h"
#include "inventory/interactions/player.h"
#include "inventory/inventory_interaction.h"
#include "items.h"
#include "nbt/nbt.h"
#include "networking/network_stream.h"
#include "world/client_pos.h"
#include "world/world.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <future>
#include <unordered_map>
#include <unordered_set>
#include <vector>

enum class ConnectionState : uint8_t {
	Handshaking,
	LoggingIn,
	VerifyingUsername, // Online-mode auth check is in flight on a worker thread
	WaitingForSpawnChunks,
	Playing
};

// Used to track when we are breaking a block
struct PendingBlockBreak {
	float damage = 0;
	BlockType lastBlock = BLOCK_AIR;
	Int3 lastBlockPos = { 0, 0, 0 };
	bool clientBreakMissed = false;
};

class Server;
struct PlayerSession {
	bp_player apiPlayer;
	NetworkStream stream;
	ClientPosition position;
	Server* server = nullptr;

	// Nether spawn finding
	bool pendingPortalPlacement = false;
	bool portalAreaLoaded = false;

	// What block we are currently breaking, if any
	std::optional<PendingBlockBreak> pendingBlockBreak = std::nullopt;

	// What our client is claiming this Tick
	std::optional<Vec3> pendingPosition = std::nullopt;
	std::optional<Vec3> pendingTeleport = std::nullopt;

	// Our player entity
	std::shared_ptr<EntityMPPlayer> entity;
	EntityTracker* entityTracker = nullptr;
	bool entityRegistered = false;
	int ticksFloating = 0;

	// rotation.x = yaw, rotation.y = pitch
	Float2 rotation = { 0.0f, 0.0f };

	// Bed respawn point. Only meaningful when hasBedSpawn is true.
	Int3 spawnPosition;
	bool hasBedSpawn = false;

	std::unordered_set<Int32_2> sentChunks;
	std::unordered_set<Int32_2> flushedChunks; // Actually written to stream

	// Chunks that were written to the stream during the last flush() call.
	std::vector<Int32_2> newlyFlushed;

	// Chunks that were unloaded during the last enqueue() call.
	std::vector<Int32_2> newlyUnloaded;

	// Block updates that arrived while the chunk was enqueued but not yet flushed.
	std::unordered_map<Int32_2, std::vector<PendingBlock>> pendingBlockChanges;

	ConnectionState connState = ConnectionState::Handshaking;
	std::string username = "(username not yet set)";
	std::string ipAddress;
	std::chrono::steady_clock::time_point lastPacketTime = std::chrono::steady_clock::now();
	static constexpr size_t CHAT_MESSAGE_LIMIT = 3;
	std::array<std::chrono::steady_clock::time_point, CHAT_MESSAGE_LIMIT> recentChatMessageTimes{};
	size_t nextChatMessageTime = 0;
	size_t recentChatMessageCount = 0;

	std::string serverId;
	std::future<bool> pendingAuthFuture;
	std::chrono::steady_clock::time_point authStartTime;

	// Inventory
	InventoryPlayer inventory;
	PlayerInventoryInteraction inventoryInteraction;
	std::unique_ptr<InventoryInteraction> activeInteraction = nullptr;

	// windowId = 0 is always the player inventory. Non-zero means a container is open.
	// ranges from 0-127 and wraps
	WindowId openWindowId = 0;
	WindowId GetNextWindowId() {
		openWindowId = (openWindowId + 1) % 128;
		if (openWindowId == 0)
			// ID 0 is reserved
			openWindowId++;
		return openWindowId;
	}

	// Lock after a rejected click until client acknowledges the resync.
	// While locked, all incoming clicks are rejected to prevent state corruption.
	bool inventoryLocked = false;
	TransactionId pendingTransactionId = 0;
	WindowId pendingWindowId = 0;

	Dimension dimension = Dimension::Overworld; // 0 = overworld, -1 = nether

	explicit PlayerSession(int _socket, Runtime& _gameRuntime)
	    : apiPlayer(this), stream(_socket), inventoryInteraction(&inventory, _gameRuntime) {}
	~PlayerSession() {
		// So our player entity despawns from the world
		if (entity) {
			entity->isDead = true;
			entity->session = nullptr;
		}
		entityTracker = nullptr;
	}

	// Load our player data from file
	void LoadPlayerNbt(Tag& _nbt) {
		if (!entity)
			return;

		if (_nbt.type != TAG_COMPOUND) {
			Logger::Error << "Player NBT data is not a compound tag!\n";
			return;
		}

		entity->LoadFromNbt(_nbt);

		dimension = _nbt.Has("Dimension") ? static_cast<Dimension>(_nbt.Get("Dimension").GetInt())
		                                  : Dimension::Overworld;

		hasBedSpawn = _nbt.Has("SpawnX") && _nbt.Has("SpawnY") && _nbt.Has("SpawnZ");
		if (hasBedSpawn) {
			spawnPosition = { _nbt.Get("SpawnX").GetInt(), _nbt.Get("SpawnY").GetInt(), _nbt.Get("SpawnZ").GetInt() };
		}

		if (_nbt.Has("Inventory")) {
			auto& it3 = _nbt.Get("Inventory").GetList();
			for (auto& item : it3) {
				NbtSlotId nbtSlot = item.Get("Slot").GetByte();
				NetworkSlotId networkSlot = inventory.GetNetworkSlotId(nbtSlot);
				if (networkSlot < 0 || networkSlot >= int(inventory.slots.size()))
					continue;
				inventory.slots[size_t(networkSlot)] = ItemStack{ item.Get("id").GetShort(), item.Get("Count").GetByte(),
					                                              item.Get("Damage").GetShort() };
			}
		}

		this->position.pos = entity->position;
		this->rotation.x = entity->rotationYaw;
		this->rotation.z = entity->rotationPitch;
	}

	Tag SerializeToNbt() {
		if (!entity)
			return {};

		auto entityNBT = entity->SerializeToNbt();
		if (!entityNBT.has_value())
			return {};

		Tag sleepTimerTag;
		sleepTimerTag.type = TAG_SHORT;
		sleepTimerTag.name = "SleepTimer";
		sleepTimerTag.shortValue = static_cast<int16_t>(entity->isSleeping ? entity->ticksInBed : 0);
		Tag dimensionTag;
		dimensionTag.type = TAG_INT;
		dimensionTag.name = "Dimension";
		dimensionTag.intValue = int(dimension);
		Tag sleepingTag;
		sleepingTag.type = TAG_BYTE;
		sleepingTag.name = "Sleeping";
		sleepingTag.byteValue = entity->isSleeping;
		Tag spawnXTag;
		spawnXTag.type = TAG_INT;
		spawnXTag.name = "SpawnX";
		spawnXTag.intValue = spawnPosition.x;
		Tag spawnYTag;
		spawnYTag.type = TAG_INT;
		spawnYTag.name = "SpawnY";
		spawnYTag.intValue = spawnPosition.y;
		Tag spawnZTag;
		spawnZTag.type = TAG_INT;
		spawnZTag.name = "SpawnZ";
		spawnZTag.intValue = spawnPosition.z;
		Tag inventoryTag;
		inventoryTag.type = TAG_LIST;
		inventoryTag.name = "Inventory";
		inventoryTag.listType = TAG_COMPOUND;

		// Save our current inventory
		NetworkSlotId slotId = 0;
		for (auto& item : inventory.slots) {
			if (item.id != Items::Id::INVALID) {
				Tag itemTag;
				itemTag.type = TAG_COMPOUND;
				itemTag.name = "";
				Tag slotTag;
				slotTag.type = TAG_BYTE;
				slotTag.name = "Slot";
				slotTag.byteValue = inventory.GetNbtSlotId(slotId);
				Tag idTag;
				idTag.type = TAG_SHORT;
				idTag.name = "id";
				idTag.shortValue = item.id;
				Tag countTag;
				countTag.type = TAG_BYTE;
				countTag.name = "Count";
				countTag.byteValue = item.count;
				Tag damageTag;
				damageTag.type = TAG_SHORT;
				damageTag.name = "Damage";
				damageTag.shortValue = item.data;

				itemTag.compound["Slot"] = slotTag;
				itemTag.compound["id"] = idTag;
				itemTag.compound["Count"] = countTag;
				itemTag.compound["Damage"] = damageTag;
				inventoryTag.list.push_back(itemTag);
			}
			slotId++;
		}

		auto& rootTag = entityNBT.value();
		rootTag.compound["SleepTimer"] = sleepTimerTag;
		rootTag.compound["Dimension"] = dimensionTag;
		rootTag.compound["Sleeping"] = sleepingTag;
		rootTag.compound["Inventory"] = inventoryTag;
		if (hasBedSpawn) {
			rootTag.compound["SpawnX"] = spawnXTag;
			rootTag.compound["SpawnY"] = spawnYTag;
			rootTag.compound["SpawnZ"] = spawnZTag;
		}
		return rootTag;
	}
};