/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once
#include "base_types.h"
#include "blocks.h"
#include "dimensions.h"
#include "entities/entity_player.h"
#include "inventory/interactions/player.h"
#include "inventory/inventories.h"
#include "inventory/inventory_interaction.h"
#include "inventory/item_stack.h"
#include "items.h"
#include "logger.h"
#include "logstream.h"
#include "nbt/nbt_tag.h"
#include "numeric_structs.h"
#include "runtime.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
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

struct PlayerSession {
	Vec3 position;

	// Override
	int viewDistanceOverride = 0;

	// Nether spawn finding
	bool pendingPortalPlacement = false;
	bool portalAreaLoaded = false;
	int lastLoggedSpawnChunkCount = -1; // so spawn-chunk progress is only logged when it changes

	// What block we are currently breaking, if any
	std::optional<PendingBlockBreak> pendingBlockBreak = std::nullopt;

	// Our player entity
	std::shared_ptr<PlayerEntity> entity;

	// rotation.x = yaw, rotation.y = pitch
	Float2 rotation = { 0.0f, 0.0f };

	ConnectionState connState = ConnectionState::Handshaking;
	std::string username = "(username not yet set)";

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

	explicit PlayerSession(Runtime& _gameRuntime) : inventoryInteraction(&inventory, _gameRuntime) {}
	virtual ~PlayerSession() {
		// So our player entity despawns from the world
		if (entity) {
			entity->isDead = true;
		}
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

		entity->hasBedSpawn = _nbt.Has("SpawnX") && _nbt.Has("SpawnY") && _nbt.Has("SpawnZ");
		if (entity->hasBedSpawn) {
			entity->spawnPosition = { _nbt.Get("SpawnX").GetInt(), _nbt.Get("SpawnY").GetInt(),
				                      _nbt.Get("SpawnZ").GetInt() };
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

		this->position = entity->position;
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
		spawnXTag.intValue = entity->spawnPosition.x;
		Tag spawnYTag;
		spawnYTag.type = TAG_INT;
		spawnYTag.name = "SpawnY";
		spawnYTag.intValue = entity->spawnPosition.y;
		Tag spawnZTag;
		spawnZTag.type = TAG_INT;
		spawnZTag.name = "SpawnZ";
		spawnZTag.intValue = entity->spawnPosition.z;
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
		if (entity->hasBedSpawn) {
			rootTag.compound["SpawnX"] = spawnXTag;
			rootTag.compound["SpawnY"] = spawnYTag;
			rootTag.compound["SpawnZ"] = spawnZTag;
		}
		return rootTag;
	}

	// I love polymorphism!!
	virtual void Reserved(){};
};