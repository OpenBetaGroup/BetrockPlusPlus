/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#include "entity_tracker.h"
#include "../server.h"
#include "direction_fixer.h"
#include "entities.h"
#include "entities/entity_arrow.h"
#include "entities/entity_egg.h"
#include "entities/entity_fishing_bobber.h"
#include "entities/entity_item.h"
#include "entities/entity_minecart.h"
#include "entities/entity_mobile.h"
#include "entities/entity_painting.h"
#include "entities/entity_snowball.h"
#include "entities/entity_fireball.h"
#include "logger.h"
#include "packet_data.h"
#include <algorithm>
#include "networking/packets.h"
#include <cstdint>

double DistanceBetweenPlayerAndEntity(Entity* _entity, Entity* _player) {
	auto dx = _entity->position.x - _player->position.x;
	auto dz = _entity->position.z - _player->position.z;
	return dx * dx + dz * dz;
}

void EntityTracker::SpawnMob(TrackedEntry& _entityEntry, const PacketData::MobType _networkEntityType, std::shared_ptr<PlayerSession> _session) {
	if (!_entityEntry.entity)
		return;
	Packet::SpawnMob pkt;
	pkt.entityId = _entityEntry.entity->id;
	pkt.mobType = _networkEntityType;
	pkt.qPosition = QuantizePosition(_entityEntry.entity->position);
	pkt.qRotation = { int8_t(QuantizeRotation(_entityEntry.entity->rotationYaw)),
						int8_t(QuantizeRotation(_entityEntry.entity->rotationPitch)) };
	_entityEntry.entity->EncodeMetadata(pkt.metadata);
	pkt.Serialize(_session->stream);
}

// Update each player instance so entities properly despawn and spawn for them
void EntityTracker::Tick() {
	std::vector<EntityId> deadThisTick;

	for (auto& [entityId, entry] : trackedEntities) {
		if (entry.entity->isDead) {
			deadThisTick.push_back(entry.entity->id);
		}
	}

	for (auto& entityId : deadThisTick) {
		auto& entry = trackedEntities.at(entityId);
		DespawnEntityForViewers(entityId, entry);
		for (auto& [id, otherEntry] : trackedEntities) {
			otherEntry.visibleTo.erase(entityId);
		}
		trackedEntities.erase(entityId);
		playerIds.erase(entityId);
	}

	// Despawn pass / update
	for (auto& [entityId, entry] : trackedEntities) {
		this->Update(entry); // Determine what packets to send
		for (auto it = entry.visibleTo.begin(); it != entry.visibleTo.end();) {
			EntityId playerId = *it;
			auto playerIt = trackedEntities.find(playerId);
			if (playerIt == trackedEntities.end()) {
				it = entry.visibleTo.erase(it);
				continue;
			}
			auto& player = playerIt->second;

			if (DistanceBetweenPlayerAndEntity(entry.entity, player.entity) >
			    entry.profile.range * entry.profile.range) {
				auto pSession = server->GetSessionById(playerId);
				if (!pSession) {
					it = entry.visibleTo.erase(it);
					continue;
				}
				Packet::DespawnEntity pkt;
				pkt.entityId = entry.entity->id;
				pkt.Serialize(pSession->stream);

				it = entry.visibleTo.erase(it);
			} else {
				++it;
			}
		}
	}

	// Spawn pass
	for (EntityId playerId : playerIds) {
		auto& player = trackedEntities.at(playerId);

		for (auto& [entityId, entityEntry] : trackedEntities) {
			if (entityId == playerId)
				continue;

			if (DistanceBetweenPlayerAndEntity(entityEntry.entity, player.entity) >
			        entityEntry.profile.range * entityEntry.profile.range ||
			    entityEntry.visibleTo.find(playerId) != entityEntry.visibleTo.end()) {
				continue;
			}

			SpawnEntityForPlayer(playerId, entityEntry);
		}
	}
}

void EntityTracker::TrackEntity(Entity* _entity) {
	TrackedEntry entry;
	entry.entity = _entity;
	entry.profile = GetTrackingProfile(*_entity);

	entry.lastEncodedPos = QuantizePosition(_entity->position);
	entry.lastBroadcastMotion = _entity->velocity;
	entry.lastEncodedPitch = QuantizeRotation(_entity->rotationPitch);
	entry.lastEncodedYaw = QuantizeRotation(_entity->rotationYaw);
	entry.updateCounter = entry.profile.updateFrequency > 0 ? rand.NextInt(entry.profile.updateFrequency) : 0;

	trackedEntities[_entity->id] = std::move(entry);

	// Let any player already in range see this entity right away
	auto& newEntry = trackedEntities.at(_entity->id);
	for (EntityId playerId : playerIds) {
		if (playerId == _entity->id)
			continue;
		auto playerIt = trackedEntities.find(playerId);
		if (playerIt == trackedEntities.end())
			continue;
		auto& player = playerIt->second;
		if (DistanceBetweenPlayerAndEntity(newEntry.entity, player.entity) >
		    newEntry.profile.range * newEntry.profile.range)
			continue;
		// Register the viewer before spawning
		newEntry.visibleTo.insert(playerId);
		SpawnEntityForPlayer(playerId, newEntry);
	}

	// Force an update
	Update(newEntry);
}

void EntityTracker::UntrackEntity(Entity* _entity) {
	auto it = trackedEntities.find(_entity->id);
	if (it == trackedEntities.end())
		return;

	DespawnEntityForViewers(_entity->id, it->second);

	for (auto& [id, otherEntry] : trackedEntities)
		otherEntry.visibleTo.erase(_entity->id);

	trackedEntities.erase(it);
	playerIds.erase(_entity->id);
}

void EntityTracker::AddPlayer(Entity* _player) {
	TrackedEntry entry;
	entry.entity = _player;
	entry.profile = GetTrackingProfile(*_player);

	entry.lastEncodedPos = QuantizePosition(_player->position);
	entry.lastBroadcastMotion = _player->velocity;
	entry.lastEncodedPitch = QuantizeRotation(_player->rotationPitch);
	entry.lastEncodedYaw = QuantizeRotation(_player->rotationYaw);
	entry.updateCounter = entry.profile.updateFrequency > 0 ? rand.NextInt(entry.profile.updateFrequency) : 0;

	trackedEntities[_player->id] = std::move(entry);
	playerIds.insert(_player->id);
	auto& newPlayerEntry = trackedEntities.at(_player->id);

	// This new player should immediately see anything already in range
	for (auto& [entityId, entityEntry] : trackedEntities) {
		if (entityId == _player->id)
			continue;
		if (DistanceBetweenPlayerAndEntity(entityEntry.entity, newPlayerEntry.entity) >
		    entityEntry.profile.range * entityEntry.profile.range)
			continue;
		// Register the viewer before spawning
		entityEntry.visibleTo.insert(_player->id);
		SpawnEntityForPlayer(_player->id, entityEntry);
	}

	for (EntityId otherPlayerId : playerIds) {
		if (otherPlayerId == _player->id)
			continue;
		auto otherIt = trackedEntities.find(otherPlayerId);
		if (otherIt == trackedEntities.end())
			continue;
		if (DistanceBetweenPlayerAndEntity(newPlayerEntry.entity, otherIt->second.entity) >
		    newPlayerEntry.profile.range * newPlayerEntry.profile.range)
			continue;
		newPlayerEntry.visibleTo.insert(otherPlayerId);
		SpawnEntityForPlayer(otherPlayerId, newPlayerEntry);
	}

	// Force an update
	Update(newPlayerEntry);
}

void EntityTracker::RemovePlayer(Entity* _player) {
	// Despawn all entities that we are viewing
	for (auto& [id, otherEntry] : trackedEntities)
		if (otherEntry.visibleTo.contains(_player->id)) {
			// Send a despawn packet to the client
			Packet::DespawnEntity pkt;
			pkt.entityId = otherEntry.entity->id;
			auto playerSession = server->GetSessionById(_player->id);
			if (playerSession)
				pkt.Serialize(playerSession->stream);
		}

	// Fully untrack this entity as usual
	UntrackEntity(_player);
}

void EntityTracker::SpawnEntityForPlayer(EntityId _playerId, TrackedEntry& _entityEntry) {
	auto pSession = server->GetSessionById(_playerId);
	if (!pSession)
		return;
	switch (_entityEntry.entity->type) {
	case EntityType::ITEM: {
		ItemEntity& ie = static_cast<ItemEntity&>(*_entityEntry.entity);
		Packet::SpawnItem pkt;
		pkt.entityId = _entityEntry.entity->id;
		pkt.item = ie.itemStack;
		pkt.qPosition = QuantizePosition(_entityEntry.entity->position);
		// For some reason notch decided this should be a convoluted way of getting the initial spawn velocity
		auto quantizeSpawnVelocity = [](double _v) -> int8_t {
			return int8_t(_v * 128.0);
		};
		pkt.qVelocity = { quantizeSpawnVelocity(_entityEntry.entity->velocity.x),
			              quantizeSpawnVelocity(_entityEntry.entity->velocity.y),
			              quantizeSpawnVelocity(_entityEntry.entity->velocity.z) };
		pkt.Serialize(pSession->stream);
		break;
	}
	case EntityType::PLAYER: {
		Packet::SpawnPlayer pkt;
		pkt.entityId = _entityEntry.entity->id;
		pkt.heldItemId = Items::Id::NONE;
		pkt.qPosition = QuantizePosition(_entityEntry.entity->position);
		pkt.qRotation = { int8_t(QuantizeRotation(_entityEntry.entity->rotationYaw)),
			              int8_t(QuantizeRotation(_entityEntry.entity->rotationPitch)) };

		// To prevent bad behavior when we share a name with another entity
		auto username = server->GetUsernameByEntityId(_entityEntry.entity->id);
		if (username.empty()) {
			// Fallback
			auto& playerEntity = static_cast<EntityMPPlayer&>(*_entityEntry.entity);
			username = playerEntity.session ? playerEntity.session->username : " ";
		}
		pkt.username = username;
		pkt.Serialize(pSession->stream);
		SendEquipmentState(_entityEntry, pSession);
		SendMetadataState(_entityEntry, pSession);

		// Sync riding state with the new joiner
		if (auto vehiclePtr = _entityEntry.entity->vehicle.lock()) {
			Packet::AddPassenger passengerPkt;
			passengerPkt.vehicleEntityId = vehiclePtr->id;
			passengerPkt.passengerEntityId = _entityEntry.entity->id;
			passengerPkt.Serialize(pSession->stream);
		}
		// If they're already sleeping, let the new joiner see them in bed.
		if (auto* mpPlayer = static_cast<EntityMPPlayer*>(_entityEntry.entity)) {
			if (mpPlayer->isSleeping) {
				Packet::InteractWithBlock sleepingAnim;
				sleepingAnim.entityId = mpPlayer->id;
				sleepingAnim.interactionId = PacketData::BlockInteraction::SLEEPING;
				sleepingAnim.position = { mpPlayer->bedPosition.x, static_cast<int8_t>(mpPlayer->bedPosition.y),
					                      mpPlayer->bedPosition.z };
				sleepingAnim.Serialize(pSession->stream);
			}
		}
		break;
	}
	case EntityType::CREEPER:
		SpawnMob(_entityEntry, PacketData::MobType::CREEPER, pSession);
		break;
	case EntityType::SPIDER:
		SpawnMob(_entityEntry, PacketData::MobType::SPIDER, pSession);
		break;
	case EntityType::ZOMBIE:
		SpawnMob(_entityEntry, PacketData::MobType::ZOMBIE, pSession);
		break;
	case EntityType::ZOMBIE_PIGMAN:
		SpawnMob(_entityEntry, PacketData::MobType::ZOMBIE_PIGMAN, pSession);
		break;
	case EntityType::SKELETON:
		SpawnMob(_entityEntry, PacketData::MobType::SKELETON, pSession);
		break;
	case EntityType::PIG:
		SpawnMob(_entityEntry, PacketData::MobType::PIG, pSession);
		break;
	case EntityType::COW:
		SpawnMob(_entityEntry, PacketData::MobType::COW, pSession);
		break;
	case EntityType::SHEEP:
		SpawnMob(_entityEntry, PacketData::MobType::SHEEP, pSession);
		break;
	case EntityType::CHICKEN:
		SpawnMob(_entityEntry, PacketData::MobType::CHICKEN, pSession);
		break;
	case EntityType::SQUID:
		SpawnMob(_entityEntry, PacketData::MobType::SQUID, pSession);
		break;
	case EntityType::WOLF:
		SpawnMob(_entityEntry, PacketData::MobType::WOLF, pSession);
		break;
	case EntityType::SLIME:
		SpawnMob(_entityEntry, PacketData::MobType::SLIME, pSession);
		break;
	case EntityType::GHAST:
		SpawnMob(_entityEntry, PacketData::MobType::GHAST, pSession);
		break;
	case EntityType::BOAT: {
		Packet::SpawnObject pkt;
		pkt.entityId = _entityEntry.entity->id;
		pkt.objectType = PacketData::ObjectType::BOAT;
		pkt.qPosition = QuantizePosition(_entityEntry.entity->position);
		pkt.qVelocity = QuantizeVelocity(_entityEntry.entity->velocity);
		pkt.Serialize(pSession->stream);

		// This is annoying, but the boat needs a separate rotation packet to be sent after the spawn packet
		Packet::EntityRotation pktRot;
		pktRot.entityId = _entityEntry.entity->id;
		pktRot.qRotation = { int8_t(QuantizeRotation(_entityEntry.entity->rotationYaw)),
			                 int8_t(QuantizeRotation(_entityEntry.entity->rotationPitch)) };
		pktRot.Serialize(pSession->stream);
		break;
	}
	case EntityType::MINECART: {
		Packet::SpawnObject pkt;
		pkt.entityId = _entityEntry.entity->id;
		// vanilla: object type is 10 + minecartType (10 = minecart, 11 = chest, 12 = furnace)
		auto cartType = static_cast<MinecartEntity*>(_entityEntry.entity)->cartType;
		pkt.objectType = PacketData::ObjectType(int(PacketData::ObjectType::MINECART) + int(cartType));
		pkt.qPosition = QuantizePosition(_entityEntry.entity->position);
		pkt.qVelocity = QuantizeVelocity(_entityEntry.entity->velocity);
		pkt.Serialize(pSession->stream);

		Packet::EntityRotation pktRot;
		pktRot.entityId = _entityEntry.entity->id;
		pktRot.qRotation = { int8_t(QuantizeRotation(_entityEntry.entity->rotationYaw)),
			                 int8_t(QuantizeRotation(_entityEntry.entity->rotationPitch)) };
		pktRot.Serialize(pSession->stream);
		break;
	}
	case EntityType::PAINTING: {
		PaintingEntity* painting = static_cast<PaintingEntity*>(_entityEntry.entity);
		Packet::SpawnPainting pkt;
		pkt.title = painting->art.title;
		pkt.entityId = painting->id;
		pkt.qPosition = painting->blockCoordinates;
		pkt.direction = FromDirectionToPaintingDirection(painting->direction);
		pkt.Serialize(pSession->stream);
		break;
	}
	case EntityType::LIGHTNING_BOLT: {
		Packet::LightningBolt pkt;
		pkt.entityType = 1;
		pkt.entityId = _entityEntry.entity->id;
		pkt.position = QuantizePosition(_entityEntry.entity->position);
		pkt.Serialize(pSession->stream);
		break;
	}
	case EntityType::FALLING_SAND: {
		Packet::SpawnObject pkt;
		pkt.entityId = _entityEntry.entity->id;
		pkt.objectType = PacketData::ObjectType::FALLING_SAND;
		pkt.qPosition = QuantizePosition(_entityEntry.entity->position);
		pkt.qVelocity = QuantizeVelocity(_entityEntry.entity->velocity);
		pkt.Serialize(pSession->stream);
		break;
	}
	case EntityType::FALLING_GRAVEL: {
		Packet::SpawnObject pkt;
		pkt.entityId = _entityEntry.entity->id;
		pkt.objectType = PacketData::ObjectType::FALLING_GRAVEL;
		pkt.qPosition = QuantizePosition(_entityEntry.entity->position);
		pkt.qVelocity = QuantizeVelocity(_entityEntry.entity->velocity);
		pkt.Serialize(pSession->stream);
		break;
	}
	case EntityType::FISHING_BOBBER: {
		auto* bobber = static_cast<FishingBobberEntity*>(_entityEntry.entity);
		auto owner = bobber ? bobber->owner.lock() : nullptr;

		if (!owner || owner->isDead)
			break;
		auto* mpOwner = static_cast<EntityMPPlayer*>(owner.get());
		if (!mpOwner || !mpOwner->session)
			break;

		Packet::SpawnObject pkt;
		pkt.entityId = _entityEntry.entity->id;
		pkt.objectType = PacketData::ObjectType::FISHING_BOBBER;
		pkt.qPosition = QuantizePosition(_entityEntry.entity->position);
		pkt.ownerEntityId = owner->id;
		pkt.qVelocity = QuantizeVelocity(_entityEntry.entity->velocity);
		pkt.Serialize(pSession->stream);
		break;
	}
	case EntityType::ARROW: {
		auto* arrow = static_cast<ArrowEntity*>(_entityEntry.entity);
		auto owner = arrow ? arrow->GetOwner() : nullptr;
		Packet::SpawnObject pkt;
		pkt.entityId = _entityEntry.entity->id;
		pkt.objectType = PacketData::ObjectType::ARROW;
		pkt.qPosition = QuantizePosition(_entityEntry.entity->position);
		// Beta sends the owner id, or the arrow's own id if it has none
		pkt.ownerEntityId = owner ? owner->id : _entityEntry.entity->id;
		pkt.qVelocity = QuantizeVelocity(_entityEntry.entity->velocity);
		pkt.Serialize(pSession->stream);
		break;
	}
	case EntityType::FIREBALL: {
		auto* fb = static_cast<FireballEntity*>(_entityEntry.entity);
		auto owner = fb ? fb->GetOwner() : nullptr;
		Packet::SpawnObject pkt;
		pkt.entityId = _entityEntry.entity->id;
		pkt.objectType = PacketData::ObjectType::FIREBALL;
		pkt.qPosition = QuantizePosition(_entityEntry.entity->position);
		// Beta sends the owner id, or the fireball's own id if it has none
		pkt.ownerEntityId = owner ? owner->id : _entityEntry.entity->id;
		pkt.qVelocity = QuantizeVelocity(fb->acceleration);
		pkt.Serialize(pSession->stream);
		break;
	}
	case EntityType::THROWN_EGG: {
		auto* egg = static_cast<EggEntity*>(_entityEntry.entity);
		auto owner = egg ? egg->GetOwner() : nullptr;
		Packet::SpawnObject pkt;
		pkt.entityId = _entityEntry.entity->id;
		pkt.objectType = PacketData::ObjectType::THROWN_EGG;
		pkt.qPosition = QuantizePosition(_entityEntry.entity->position);
		pkt.ownerEntityId = owner ? owner->id : _entityEntry.entity->id;
		pkt.qVelocity = QuantizeVelocity(_entityEntry.entity->velocity);
		pkt.Serialize(pSession->stream);
		break;
	}
	case EntityType::THROWN_SNOWBALL: {
		auto* snowball = static_cast<SnowballEntity*>(_entityEntry.entity);
		auto owner = snowball ? snowball->GetOwner() : nullptr;
		Packet::SpawnObject pkt;
		pkt.entityId = _entityEntry.entity->id;
		pkt.objectType = PacketData::ObjectType::THROWN_SNOWBALL;
		pkt.qPosition = QuantizePosition(_entityEntry.entity->position);
		pkt.ownerEntityId = owner ? owner->id : _entityEntry.entity->id;
		pkt.qVelocity = QuantizeVelocity(_entityEntry.entity->velocity);
		pkt.Serialize(pSession->stream);
		break;
	}
	case EntityType::LIT_TNT: {
		Packet::SpawnObject pkt;
		pkt.entityId = _entityEntry.entity->id;
		pkt.objectType = PacketData::ObjectType::LIT_TNT;
		pkt.qPosition = QuantizePosition(_entityEntry.entity->position);
		pkt.qVelocity = QuantizeVelocity(_entityEntry.entity->velocity);
		pkt.Serialize(pSession->stream);
		break;
	}
	default:
		Logger::Warning << "Unhandled entity type: " << int(_entityEntry.entity->type) << "\n";
		return;
	}
	_entityEntry.visibleTo.insert(_playerId);

	if (_entityEntry.profile.sendVelocity) {
		// If velocity is enabled immediately send a follow up
		Packet::EntityVelocity velPkt;
		velPkt.entityId = _entityEntry.entity->id;
		velPkt.velocity = QuantizeVelocity(_entityEntry.entity->velocity);
		velPkt.Serialize(pSession->stream);
	}
}

void EntityTracker::DespawnEntityForViewers(EntityId _entityId, TrackedEntry& _entry) {
	for (EntityId viewerId : _entry.visibleTo) {
		auto pSession = server->GetSessionById(viewerId);
		if (!pSession)
			continue;
		Packet::DespawnEntity pkt;
		pkt.entityId = _entityId;
		pkt.Serialize(pSession->stream);
	}
}

void EntityTracker::SendPacketToPlayersInTrackedEntry(Packet::BasePacket& _pkt, TrackedEntry& _trackedEntry) {
	for (EntityId playerId : _trackedEntry.visibleTo) {
		auto playerIt = trackedEntities.find(playerId);
		if (playerIt == trackedEntities.end()) continue;

		auto* player = static_cast<EntityMPPlayer*>(playerIt->second.entity);
		if (!player || !player->session) continue;
		_pkt.Serialize(player->session->stream);
	}
}

void EntityTracker::SendPacketToAllPlayers(Packet::BasePacket& _pkt) {
	for (auto& player : this->playerIds) {
		auto session = server->GetSessionById(player);
		if (!session)
			continue;
		_pkt.Serialize(session->stream);
	}
}

TrackedEntry* EntityTracker::GetTrackerForEntityId(EntityId _id) {
	auto it = trackedEntities.find(_id);
	return it != trackedEntities.end() ? &it->second : nullptr;
}

void EntityTracker::SendPacketToViewers(Packet::BasePacket& _pkt, EntityId _id) {
	auto* entry = GetTrackerForEntityId(_id);
	if (!entry)
		return; // entity isn't tracked
	SendPacketToPlayersInTrackedEntry(_pkt, *entry);
}

void EntityTracker::SendMetadataState(TrackedEntry& _trackedEntry, std::shared_ptr<PlayerSession> _targetSession) {
	Packet::EntityMetadata pkt;
	pkt.entityId = _trackedEntry.entity->id;
	_trackedEntry.entity->EncodeMetadata(pkt.metadata);
	pkt.Serialize(_targetSession->stream);
}

void EntityTracker::UpdateMetadataState(TrackedEntry& _trackedEntry) {
	if (!_trackedEntry.entity->wasMetadataUpdated)
		return;
	Packet::EntityMetadata pkt;
	pkt.entityId = _trackedEntry.entity->id;
	_trackedEntry.entity->EncodeMetadata(pkt.metadata);
	SendPacketToViewers(pkt, _trackedEntry.entity->id);
	_trackedEntry.entity->wasMetadataUpdated = false;

	// Send the packet to ourselves if we are a player
	auto mpe = dynamic_cast<EntityMPPlayer*>(_trackedEntry.entity);
	if (mpe && mpe->session) {
		pkt.Serialize(mpe->session->stream);
	}
}

// Used when we first spawn an entity for a specific player
void EntityTracker::SendEquipmentState(TrackedEntry& _trackedEntry, std::shared_ptr<PlayerSession> _targetSession) {
	auto updateEquipmentSlot = [&](int _slot, ItemStack* _stack) -> void {
		if (!_stack || !_targetSession)
			return;
		Packet::SetEquipment pkt;
		pkt.entityId = _trackedEntry.entity->id;
		pkt.inventorySlot = _slot;
		pkt.itemId = _stack->id;
		pkt.itemMetadata = _stack->data;

		pkt.Serialize(_targetSession->stream);
	};

	MobileEntity& entity = static_cast<MobileEntity&>(*_trackedEntry.entity);
	updateEquipmentSlot(4, entity.armor[0]);
	updateEquipmentSlot(3, entity.armor[1]);
	updateEquipmentSlot(2, entity.armor[2]);
	updateEquipmentSlot(1, entity.armor[3]);
	updateEquipmentSlot(0, &entity.heldItem);
}

// Update our equipment
void EntityTracker::UpdateEquipmentState(TrackedEntry& _trackedEntry) {
	auto updateEquipmentSlot = [&](int _slot, ItemStack* _stack) -> void {
		if (!_stack)
			return;
		Packet::SetEquipment pkt;
		pkt.entityId = _trackedEntry.entity->id;
		pkt.inventorySlot = _slot;
		pkt.itemId = _stack->id;
		pkt.itemMetadata = _stack->data;

		SendPacketToViewers(pkt, _trackedEntry.entity->id);
	};

	auto& equipment = _trackedEntry.equipmentProfile;
	MobileEntity& entity = static_cast<MobileEntity&>(*_trackedEntry.entity);
	ItemStack none = {};
	auto helmet = entity.armor[0] ? entity.armor[0] : &none;
	auto chest = entity.armor[1] ? entity.armor[1] : &none;
	auto leg = entity.armor[2] ? entity.armor[2] : &none;
	auto boot = entity.armor[3] ? entity.armor[3] : &none;
	if (equipment.helmet != helmet->id) {
		updateEquipmentSlot(4, helmet);
		equipment.helmet = helmet->id;
	}
	if (equipment.chestplate != chest->id) {
		updateEquipmentSlot(3, chest);
		equipment.chestplate = chest->id;
	}
	if (equipment.legging != leg->id) {
		updateEquipmentSlot(2, leg);
		equipment.legging = leg->id;
	}
	if (equipment.boot != boot->id) {
		updateEquipmentSlot(1, boot);
		equipment.boot = boot->id;
	}
	if (equipment.heldItem != entity.heldItem) {
		updateEquipmentSlot(0, &entity.heldItem);
		equipment.heldItem = entity.heldItem;
	}
}

void EntityTracker::FlushEventQueue(Entity& _entity) {
	auto isPlayer = dynamic_cast<EntityMPPlayer*>(&_entity);

	while (_entity.pendingEvents.size() > 0) {
		auto event = _entity.pendingEvents.back();
		Packet::EntityEvent pkt;
		pkt.action = event;
		pkt.entityId = _entity.id;

		SendPacketToViewers(pkt, _entity.id);

		if (isPlayer) {
			pkt.Serialize(isPlayer->session->stream);
		}

		_entity.pendingEvents.pop_back();
	}
}

// TODO: Unnest this a bit
void EntityTracker::Update(TrackedEntry& _trackedEntry) {
	if (_trackedEntry.profile.updateFrequency == INT_MAX)
		return;
	auto& entity = _trackedEntry.entity;

	// If we are a mobile entity then send our equipment
	// TODO: Only send these if any of them have been updated
	if (dynamic_cast<MobileEntity*>(entity)) {
		FlushEventQueue(*entity);
		UpdateEquipmentState(_trackedEntry);
		UpdateMetadataState(_trackedEntry);
	}

	// Dirty flag gets checked every Tick
	if (entity->forceVelocityUpdate) {
		entity->forceVelocityUpdate = false;
		_trackedEntry.lastBroadcastMotion = { entity->velocity.x, entity->velocity.y, entity->velocity.z };
		Packet::EntityVelocity pkt;
		pkt.entityId = entity->id;
		pkt.velocity = { QuantizeVelocityComponent(entity->velocity.x), QuantizeVelocityComponent(entity->velocity.y),
			             QuantizeVelocityComponent(entity->velocity.z) };
		SendPacketToPlayersInTrackedEntry(pkt, _trackedEntry);

		// If we are a player then we need to recieve this velocity update
		if (auto thisSession = server->GetSessionById(entity->id)) {
			pkt.Serialize(thisSession->stream);
		}
	}

	_trackedEntry.ticksSinceTeleport++;
	_trackedEntry.updateCounter++;

	bool needsMovementUpdate = _trackedEntry.updateCounter >= _trackedEntry.profile.updateFrequency ||
	                           _trackedEntry.ticksSinceTeleport >= forceTeleportTicks;

	// Determine if we should send our velocity
	bool needsVelocityUpdate = false;

	constexpr double THRESHOLD = 0.01;
	Vec3 currentMotion;
	currentMotion.x = std::abs(entity->velocity.x) < THRESHOLD ? 0 : entity->velocity.x;
	currentMotion.y = std::abs(entity->velocity.y) < THRESHOLD ? 0 : entity->velocity.y;
	currentMotion.z = std::abs(entity->velocity.z) < THRESHOLD ? 0 : entity->velocity.z;
	Vec3& lastMotion = _trackedEntry.lastBroadcastMotion;

	if (_trackedEntry.profile.sendVelocity) {
		Vec3 delta = currentMotion - lastMotion;
		const double deltaLen = delta.Length();
		const double motionThreshold = 0.02;

		needsVelocityUpdate = (deltaLen > motionThreshold && needsMovementUpdate) ||
		                      (deltaLen > 0.0 && currentMotion.x == 0.0 && currentMotion.y == 0.0 &&
		                       currentMotion.z == 0.0);
	}

	if (needsVelocityUpdate) {
		lastMotion = currentMotion;
		Packet::EntityVelocity pkt;
		pkt.entityId = entity->id;
		pkt.velocity = { QuantizeVelocityComponent(currentMotion.x), QuantizeVelocityComponent(currentMotion.y),
			             QuantizeVelocityComponent(currentMotion.z) };
		SendPacketToPlayersInTrackedEntry(pkt, _trackedEntry);
	}

	if (needsMovementUpdate) {
		_trackedEntry.updateCounter = 0;

		const int32_t qx = QuantizePositionComponent(entity->position.x);
		const int32_t qy = QuantizePositionComponent(entity->position.y);
		const int32_t qz = QuantizePositionComponent(entity->position.z);
		const int32_t qYaw = QuantizeRotation(entity->rotationYaw);
		const int32_t qPitch = QuantizeRotation(entity->rotationPitch);

		const int32_t dx = qx - _trackedEntry.lastEncodedPos.x;
		const int32_t dy = qy - _trackedEntry.lastEncodedPos.y;
		const int32_t dz = qz - _trackedEntry.lastEncodedPos.z;

		const bool needsTP = dx < -128 || dx >= 128 || dy < -128 || dy >= 128 || dz < -128 || dz >= 128 ||
		                     _trackedEntry.ticksSinceTeleport >= forceTeleportTicks;

		if (needsTP) {
			_trackedEntry.ticksSinceTeleport = 0;

			Packet::TeleportEntity pkt;
			pkt.entityId = entity->id;
			pkt.qPosition = { qx, qy, qz };
			pkt.qRotation = { int8_t(qYaw), int8_t(qPitch) };
			SendPacketToPlayersInTrackedEntry(pkt, _trackedEntry);
			_trackedEntry.lastEncodedPos = { qx, qy, qz };
			_trackedEntry.lastEncodedYaw = qYaw;
			_trackedEntry.lastEncodedPitch = qPitch;
		} else {
			// Vehicles always get a position packet, even when they haven't moved
			const bool needsRelMove = _trackedEntry.profile.alwaysSendPosition ||
			                          std::abs(dx) > MINIMUM_POSITION_DELTA || std::abs(dy) > MINIMUM_POSITION_DELTA ||
			                          std::abs(dz) > MINIMUM_POSITION_DELTA;
			// Only apply rotation threshold if tracked entity profile allows it
			const auto rotationThreshold = _trackedEntry.profile.applyRotationThreshold ? MINIMUM_ROTATION_DELTA : 0;
			const bool needsRot = (std::abs(qYaw - _trackedEntry.lastEncodedYaw) > rotationThreshold) ||
			                      (std::abs(qPitch - _trackedEntry.lastEncodedPitch) > rotationThreshold);

			if (needsRelMove && needsRot) {
				Packet::EntityPositionAndRotation pkt;
				pkt.qrPosition = { int8_t(dx), int8_t(dy), int8_t(dz) };
				pkt.qRotation = { int8_t(qYaw), int8_t(qPitch) };
				pkt.entityId = entity->id;
				SendPacketToPlayersInTrackedEntry(pkt, _trackedEntry);
				_trackedEntry.lastEncodedPos = { qx, qy, qz };
				_trackedEntry.lastEncodedYaw = qYaw;
				_trackedEntry.lastEncodedPitch = qPitch;
				return;
			};
			if (needsRelMove) {
				Packet::EntityPosition pkt;
				pkt.qrPosition = { int8_t(dx), int8_t(dy), int8_t(dz) };
				pkt.entityId = entity->id;
				SendPacketToPlayersInTrackedEntry(pkt, _trackedEntry);
				_trackedEntry.lastEncodedPos = { qx, qy, qz };
				return;
			}
			if (needsRot) {
				Packet::EntityRotation pkt;
				pkt.qRotation = { int8_t(qYaw), int8_t(qPitch) };
				pkt.entityId = entity->id;
				SendPacketToPlayersInTrackedEntry(pkt, _trackedEntry);
				_trackedEntry.lastEncodedYaw = qYaw;
				_trackedEntry.lastEncodedPitch = qPitch;
				return;
			}
		}
	}
}