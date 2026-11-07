/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once
#include "base_types.h"
#include "entities.h"
#include "entities/entity_manager.h"
#include "inventory/item_stack.h"
#include "logger.h"
#include "networking/network_stream.h"
#include "networking/packets.h"
#include "world/world.h"
#include <cstdio>
#include <string>
#include <unordered_map>
#include <unordered_set>

// Entity tracker so we can send entity updates to the right players. This is server side only annoyingly enough.
// I am not entirely happy with how this is done but notch demands we have several packet types for each type of entity
struct TrackingProfile {
	int range = 0;
	int updateFrequency = 0; // ticks between movement-sync packets
	bool sendVelocity = false;
	bool applyRotationThreshold = true;
	bool alwaysSendPosition = false; // send a move every updateFrequency ticks even if unchanged
};

struct EquipmentProfile {
	// Track the item ids of our equipment, we dont care about metadata here
	ItemId helmet = Items::Id::INVALID;
	ItemId chestplate = Items::Id::INVALID;
	ItemId legging = Items::Id::INVALID;
	ItemId boot = Items::Id::INVALID;
	ItemStack heldItem{};
};

struct TrackedEntry {
	Entity* entity = nullptr;
	EquipmentProfile equipmentProfile{};
	TrackingProfile profile{};
	Int32_3 lastEncodedPos{};
	Vec3 lastBroadcastMotion{};
	int32_t lastEncodedYaw = 0;
	int32_t lastEncodedPitch = 0;
	int updateCounter = 0;
	int ticksSinceTeleport = 0;
	std::unordered_set<EntityId> visibleTo; // what player ids can see this entity
};

class Server;
struct MobileEntity;
struct PlayerSession;
struct EntityTracker {
	static constexpr int32_t MINIMUM_POSITION_DELTA = 2; // 2 = 1/16th of a block (Vanilla value: 8 (1/4th of a block))
	static constexpr int32_t MINIMUM_ROTATION_DELTA = 8; // 8 Quantized angles
	Server* server = nullptr;

	std::unordered_map<EntityId, TrackedEntry> trackedEntities;
	std::unordered_set<EntityId> playerIds;

	TickTime forceTeleportTicks = 400; // 20 seconds
	Java::Random rand;

	void Tick();

	int16_t QuantizeVelocityComponent(double _v) {
		_v = std::clamp(_v, -3.9, 3.9);
		return int16_t(_v * 8000.0);
	}
	int32_t QuantizePositionComponent(double _p) {
		return MathHelper::FloorDouble(_p * 32.0);
	}

	Int16_3 QuantizeVelocity(Vec3 _v) {
		return { QuantizeVelocityComponent(_v.x), QuantizeVelocityComponent(_v.y), QuantizeVelocityComponent(_v.z) };
	}

	Int32_3 QuantizePosition(Vec3 _p) {
		return { QuantizePositionComponent(_p.x), QuantizePositionComponent(_p.y), QuantizePositionComponent(_p.z) };
	}

	int32_t QuantizeRotation(float _r) {
		return MathHelper::FloorFloat(_r * 256.0f / 360.0f);
	}

	void TrackEntity(Entity* _entity);
	void UntrackEntity(Entity* _entity);
	void AddPlayer(Entity* _player);
	void RemovePlayer(Entity* _player);
	void SpawnEntityForPlayer(EntityId _playerId, TrackedEntry& _entityEntry);
	void DespawnEntityForViewers(EntityId _entityId, TrackedEntry& _entry);
	void SendPacketToPlayersInTrackedEntry(Packet::BasePacket& _pkt, TrackedEntry& _trackedEntry);
	void SendPacketToViewers(Packet::BasePacket& _pkt, EntityId _id);
	void SendPacketToAllPlayers(Packet::BasePacket& _pkt);
	TrackedEntry* GetTrackerForEntityId(EntityId _id);
	void Update(TrackedEntry& _trackedEntry);
	void FlushEventQueue(Entity& _entity);
	void UpdateMetadataState(TrackedEntry& _trackedEntry);
	void SendMetadataState(TrackedEntry& _trackedEntry, std::shared_ptr<PlayerSession> _targetSession);
	void UpdateEquipmentState(TrackedEntry& _trackedEntry);
	void SendEquipmentState(TrackedEntry& _trackedEntry, std::shared_ptr<PlayerSession> _targetSession);
	void SpawnMob(TrackedEntry& _entityEntry, const PacketData::MobType _networkEntityType, std::shared_ptr<PlayerSession> _session);

	// With my strict goal of keeping strict separation we cannot put this as a virtual in the actual entity class itself
	TrackingProfile GetTrackingProfile(Entity& _entity) {
		const EntityType& type = _entity.type;
		switch (type) {
		case EntityType::NONE:
			return { 0, 0, /*SendVelocity=*/false };
		case EntityType::PLAYER:
			return { 128, 2, /*SendVelocity=*/false };
		case EntityType::FISHING_BOBBER:
		case EntityType::FISH:
			return { 64, 5, /*SendVelocity=*/true };
		case EntityType::ARROW:
			return { 64, 20, /*SendVelocity=*/false };
		case EntityType::FIREBALL:
			return { 64, 10, /*SendVelocity=*/false };
		case EntityType::THROWN_SNOWBALL:
		case EntityType::THROWN_EGG:
			return { 64, 10, /*SendVelocity=*/true };
		case EntityType::ITEM:
			return { 64, 20, /*SendVelocity=*/true };
		case EntityType::MINECART:
		case EntityType::BOAT:
			return { 128, 2, /*SendVelocity=*/true, /*Apply rotation threshold=*/false, /*Always send position=*/true };
		case EntityType::SQUID:
			return { 128, 3, /*SendVelocity=*/true };
		case EntityType::CHICKEN:
		case EntityType::COW:
		case EntityType::PIG:
		case EntityType::SHEEP:
		case EntityType::WOLF:
		case EntityType::ZOMBIE:
		case EntityType::ZOMBIE_PIGMAN:
		case EntityType::SKELETON:
		case EntityType::CREEPER:
		case EntityType::SPIDER:
		case EntityType::GHAST:
		case EntityType::GIANT_ZOMBIE:
			return { 128, 3, /*SendVelocity=*/false };
		case EntityType::SLIME:
			return { 128, 3, /*SendVelocity=*/false };
		case EntityType::LIT_TNT:
			return { 160, 10, /*SendVelocity=*/true };
		case EntityType::FALLING_SAND:
		case EntityType::FALLING_GRAVEL:
			return { 128, 20, /*SendVelocity=*/false };
		case EntityType::PAINTING:
			// Paintings never move so there's nothing to resync
			return { 128, INT_MAX, false };
		case EntityType::LIGHTNING_BOLT:
			return { 512, INT_MAX, false };
		default:
			return { 0, 0, false };

			Logger::Warning << "EntityTracker: no tracking profile for entity type '" + std::to_string(int(type)) +
			                           "'\n";
		}
	}
};