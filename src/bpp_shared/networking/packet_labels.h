/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once
#include <packet_ids.h>
#include <string>

inline std::string PacketIdToLabel(PacketId _id) {
	switch (_id) {
	case PacketId::KeepAlive:
		return "KeepAlive";
	case PacketId::Login:
		return "Login";
	case PacketId::PreLogin:
		return "PreLogin";
	case PacketId::ChatMessage:
		return "ChatMessage";
	case PacketId::SetTime:
		return "SetTime";
	case PacketId::SetEquipment:
		return "SetEquipment";
	case PacketId::SetSpawnPosition:
		return "SetSpawnPosition";
	case PacketId::InteractWithEntity:
		return "InteractWithEntity";
	case PacketId::SetHealth:
		return "SetHealth";
	case PacketId::Respawn:
		return "Respawn";
	case PacketId::PlayerMovement:
		return "PlayerMovement";
	case PacketId::PlayerPosition:
		return "PlayerPosition";
	case PacketId::PlayerRotation:
		return "PlayerRotation";
	case PacketId::PlayerPositionAndRotation:
		return "PlayerPositionAndRotation";
	case PacketId::MineBlock:
		return "MineBlock";
	case PacketId::PlaceBlock:
		return "PlaceBlock";
	case PacketId::SetHotbarSlot:
		return "SetHotbarSlot";
	case PacketId::InteractWithBlock:
		return "InteractWithBlock";
	case PacketId::Animation:
		return "Animation";
	case PacketId::PlayerAction:
		return "PlayerAction";
	case PacketId::SpawnPlayer:
		return "SpawnPlayer";
	case PacketId::SpawnItem:
		return "SpawnItem";
	case PacketId::CollectItem:
		return "CollectItem";
	case PacketId::SpawnObject:
		return "SpawnObject";
	case PacketId::SpawnMob:
		return "SpawnMob";
	case PacketId::SpawnPainting:
		return "SpawnPainting";
	case PacketId::PlayerInput:
		return "PlayerInput";
	case PacketId::EntityVelocity:
		return "EntityVelocity";
	case PacketId::DespawnEntity:
		return "DespawnEntity";
	case PacketId::EntityMovement:
		return "EntityMovement";
	case PacketId::EntityPosition:
		return "EntityPosition";
	case PacketId::EntityRotation:
		return "EntityRotation";
	case PacketId::EntityPositionAndRotation:
		return "EntityPositionAndRotation";
	case PacketId::TeleportEntity:
		return "TeleportEntity";
	case PacketId::EntityEvent:
		return "EntityEvent";
	case PacketId::AddPassenger:
		return "AddPassenger";
	case PacketId::EntityMetadata:
		return "EntityMetadata";
	case PacketId::SetChunkVisibility:
		return "SetChunkVisibility";
	case PacketId::Chunk:
		return "Chunk";
	case PacketId::SetMultipleBlocks:
		return "SetMultipleBlocks";
	case PacketId::SetBlock:
		return "SetBlock";
	case PacketId::BlockEvent:
		return "BlockEvent";
	case PacketId::Explosion:
		return "Explosion";
	case PacketId::WorldEvent:
		return "WorldEvent";
	case PacketId::GameEvent:
		return "GameEvent";
	case PacketId::LightningBolt:
		return "LightningBolt";
	case PacketId::OpenContainer:
		return "OpenContainer";
	case PacketId::CloseContainer:
		return "CloseContainer";
	case PacketId::ClickSlot:
		return "ClickSlot";
	case PacketId::SetSlot:
		return "SetSlot";
	case PacketId::FillContainer:
		return "FillContainer";
	case PacketId::ContainerData:
		return "ContainerData";
	case PacketId::ContainerTransaction:
		return "ContainerTransaction";
	case PacketId::UpdateSign:
		return "UpdateSign";
	case PacketId::ItemData:
		return "ItemData";
	case PacketId::IncrementStatistic:
		return "IncrementStatistic";
	case PacketId::Disconnect:
		return "Disconnect";
	default:
		return "InvalidPacket";
	}
}