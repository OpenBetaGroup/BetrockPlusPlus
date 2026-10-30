/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once
#include "base_structs.h"
#include "base_types.h"
#include "constants.h"
#include "inventory/item_stack.h"
#include "network_stream.h"
#include "numeric_structs.h"
#include "packet_data.h"
#include "quantized_types.h"
#include <dimensions.h>
#include <packet_ids.h>
#include <string>
#include <vector>

// This class serves as a nice, convenient wrapper
// around the networking packets

namespace Packet {
// NOTE: The base packet should never be used directly!!
// Only public so that packets can be passed through functions
struct BasePacket {
	PacketId id;
	BasePacket(PacketId _id) : id(_id) {}
	virtual ~BasePacket() = default;
	virtual void Serialize(NetworkStream& _stream) const = 0;
	virtual void Deserialize(NetworkStream& _stream) = 0;
};

// Used to keep the connection alive
struct KeepAlive : BasePacket {
	KeepAlive() : BasePacket{ PacketId::KeepAlive } {}

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
	}

	// NOTE: Reading the packet id is enough to deserialize it
	void Deserialize(NetworkStream& /*_stream*/) override {}
};

// Used to finalize the connection
struct Login : BasePacket {
	Login() : BasePacket{ PacketId::Login } {}
	// NOTE: This assumes that EntityId is always an int32_t
	// Client -> server: this field carries the protocol version.
	// Server -> client: this field carries the entity id.
	EntityId entityIdProtocolVersion = 0;

	std::string username;
	int64_t worldSeed = 0;
	Dimension dimension{};

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(entityIdProtocolVersion);
		_stream.WriteString16(username);
		_stream.Write(worldSeed);
		_stream.Write(dimension);
	}

	void Deserialize(NetworkStream& _stream) override {
		entityIdProtocolVersion = _stream.Read<EntityId>();
		username = _stream.ReadString16();
		worldSeed = _stream.Read<int64_t>();
		dimension = _stream.Read<Dimension>();
	}
};

// Used to initialize to connection
struct PreLogin : BasePacket {
	PreLogin() : BasePacket{ PacketId::PreLogin } {}
	// Client -> server: username. Server -> client: server id (auth hash, or "-").
	std::string usernameServerId;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.WriteString16(usernameServerId);
	}

	void Deserialize(NetworkStream& _stream) override {
		usernameServerId = _stream.ReadString16();
	}
};

// Holds a chat message
struct ChatMessage : BasePacket {
	ChatMessage() : BasePacket{ PacketId::ChatMessage } {}
	std::string message;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.WriteString16(message);
	}

	void Deserialize(NetworkStream& _stream) override {
		message = _stream.ReadString16();
	}
};

// Holds the current time
struct SetTime : BasePacket {
	SetTime() : BasePacket{ PacketId::SetTime } {}
	TickTime time;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(time);
	}

	void Deserialize(NetworkStream& _stream) override {
		time = _stream.Read<TickTime>();
	}
};

// Defines a player's equipment
struct SetEquipment : BasePacket {
	SetEquipment() : BasePacket{ PacketId::SetEquipment } {}
	EntityId entityId;
	NetworkSlotId inventorySlot;
	ItemId itemId;
	ItemDamage itemMetadata;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(entityId);
		_stream.Write(inventorySlot);
		_stream.Write(itemId);
		_stream.Write(itemMetadata);
	}

	void Deserialize(NetworkStream& _stream) override {
		entityId = _stream.Read<EntityId>();
		inventorySlot = _stream.Read<NetworkSlotId>();
		itemId = _stream.Read<ItemId>();
		itemMetadata = _stream.Read<ItemDamage>();
	}
};

// Defines where the compass points
struct SetSpawnPosition : BasePacket {
	SetSpawnPosition() : BasePacket{ PacketId::SetSpawnPosition } {}
	Int32_3 position;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(position.x);
		_stream.Write(position.y);
		_stream.Write(position.z);
	}

	void Deserialize(NetworkStream& _stream) override {
		position.x = _stream.Read<int32_t>();
		position.y = _stream.Read<int32_t>();
		position.z = _stream.Read<int32_t>();
	}
};

// Used to convey who interacted with who
struct InteractWithEntity : BasePacket {
	InteractWithEntity() : BasePacket{ PacketId::InteractWithEntity } {}
	EntityId sourceEntityId;
	EntityId targetEntityId;
	bool attack; // Usually sent when left-clicking

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(sourceEntityId);
		_stream.Write(targetEntityId);
		_stream.Write(attack);
	}

	void Deserialize(NetworkStream& _stream) override {
		sourceEntityId = _stream.Read<EntityId>();
		targetEntityId = _stream.Read<EntityId>();
		attack = _stream.Read<bool>();
	}
};

// Defines a player's health
struct SetHealth : BasePacket {
	SetHealth() : BasePacket{ PacketId::SetHealth } {}
	int16_t health;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(health);
	}

	void Deserialize(NetworkStream& _stream) override {
		health = _stream.Read<int16_t>();
	}
};

// Defines a player's health
struct Respawn : BasePacket {
	Respawn() : BasePacket{ PacketId::Respawn } {}
	Dimension dimension;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(dimension);
	}

	void Deserialize(NetworkStream& _stream) override {
		dimension = _stream.Read<Dimension>();
	}
};

// Base Packet for player movement packets
struct PlayerMovement : BasePacket {
	PlayerMovement() : BasePacket{ PacketId::PlayerMovement } {}
	bool onGround;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(onGround);
	}

	void Deserialize(NetworkStream& _stream) override {
		onGround = _stream.Read<bool>();
	}
};

// Defines the players position
struct PlayerPosition : BasePacket {
	// NOTE: notch is a silly billy!
	// When sending packets from Server -> Client, cameraY and position.y are FLIPPED!
	// This means cameraY becomes the foot position and position.y becomes the eye height.
	// HOWEVER, when recieving this packet (Client -> Server), it is correctly interpreted as position.y
	// being foot height and cameraY being eye height.

	// Why did I leave such a long comment explaining this explicitly?
	// Because this has caused me much pain.
	PlayerPosition() : BasePacket{ PacketId::PlayerPosition } {}
	Vec3 position = { 0, 0, 0 };
	double cameraY = 0;
	bool onGround = false;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(position.x);
		_stream.Write(position.y);
		_stream.Write(cameraY);
		_stream.Write(position.z);
		_stream.Write(onGround);
	}

	void Deserialize(NetworkStream& _stream) override {
		position.x = _stream.Read<double>();
		position.y = _stream.Read<double>();
		cameraY = _stream.Read<double>();
		position.z = _stream.Read<double>();
		onGround = _stream.Read<bool>();
	}
};

// Defines the players rotation
struct PlayerRotation : BasePacket {
	PlayerRotation() : BasePacket{ PacketId::PlayerRotation } {}
	union {
		Float2 rotation = { 0.0f, 0.0f };
		struct {
			float yaw, pitch;
		};
	};
	bool onGround = false;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(yaw);
		_stream.Write(pitch);
		_stream.Write(onGround);
	}

	void Deserialize(NetworkStream& _stream) override {
		yaw = _stream.Read<float>();
		pitch = _stream.Read<float>();
		onGround = _stream.Read<bool>();
	}
};

// Defines the players position and rotation
struct PlayerPositionAndRotation : BasePacket {
	PlayerPositionAndRotation() : BasePacket{ PacketId::PlayerPositionAndRotation } {}
	Vec3 position = { 0, 0, 0 };
	double cameraY = position.y + 1.62;
	union {
		Float2 rotation = { 0.0f, 0.0f };
		struct {
			float yaw, pitch;
		};
	};
	bool onGround = false;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(position.x);
		_stream.Write(position.y);
		_stream.Write(cameraY);
		_stream.Write(position.z);
		_stream.Write(yaw);
		_stream.Write(pitch);
		_stream.Write(onGround);
	}

	void Deserialize(NetworkStream& _stream) override {
		position.x = _stream.Read<double>();
		position.y = _stream.Read<double>();
		cameraY = _stream.Read<double>();
		position.z = _stream.Read<double>();
		yaw = _stream.Read<float>();
		pitch = _stream.Read<float>();
		onGround = _stream.Read<bool>();
	}
};

// Information on how far along the player is with breaking a block
struct MineBlock : BasePacket {
	MineBlock() : BasePacket{ PacketId::MineBlock } {}
	PacketData::MineStatus status;
	SlimInt3<int8_t> position;
	PacketData::FaceDirection face;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(status);
		_stream.Write(position.x);
		_stream.Write(position.y);
		_stream.Write(position.z);
		_stream.Write(face);
	}

	void Deserialize(NetworkStream& _stream) override {
		status = _stream.Read<PacketData::MineStatus>();
		position.x = _stream.Read<int32_t>();
		position.y = _stream.Read<int8_t>();
		position.z = _stream.Read<int32_t>();
		face = _stream.Read<PacketData::FaceDirection>();
	}
};

// Information on where a player is placing a block
struct PlaceBlock : BasePacket {
	PlaceBlock() : BasePacket{ PacketId::PlaceBlock } {}
	SlimInt3<int8_t> position;
	PacketData::FaceDirection face;
	ItemStack item;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(position.x);
		_stream.Write(position.y);
		_stream.Write(position.z);
		_stream.Write(face);
		_stream.Write(item.id);
		_stream.Write(item.count);
		_stream.Write(item.data);
	}

	void Deserialize(NetworkStream& _stream) override {
		position.x = _stream.Read<int32_t>();
		position.y = _stream.Read<int8_t>();
		position.z = _stream.Read<int32_t>();
		face = _stream.Read<PacketData::FaceDirection>();
		item.id = _stream.Read<ItemId>();
		if (static_cast<int16_t>(item.id) >= 0) {
			item.count = _stream.Read<ItemAmount>();
			item.data = _stream.Read<ItemDamage>();
		}
	}
};

// The clients active hotbar slot
struct SetHotbarSlot : BasePacket {
	SetHotbarSlot() : BasePacket{ PacketId::SetHotbarSlot } {}
	NetworkSlotId slot;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(slot);
	}

	void Deserialize(NetworkStream& _stream) override {
		slot = _stream.Read<NetworkSlotId>();
	}
};

// Interactions with blocks
struct InteractWithBlock : BasePacket {
	InteractWithBlock() : BasePacket{ PacketId::InteractWithBlock } {}
	EntityId entityId;
	PacketData::BlockInteraction interactionId;
	SlimInt3<int8_t> position;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(entityId);
		_stream.Write(interactionId);
		_stream.Write(position.x);
		_stream.Write(position.y);
		_stream.Write(position.z);
	}

	void Deserialize(NetworkStream& _stream) override {
		entityId = _stream.Read<EntityId>();
		interactionId = _stream.Read<PacketData::BlockInteraction>();
		position.x = _stream.Read<int32_t>();
		position.y = _stream.Read<int8_t>();
		position.z = _stream.Read<int32_t>();
	}
};

// Informs of the desired animation
struct Animation : BasePacket {
	Animation() : BasePacket{ PacketId::Animation } {}
	EntityId entityId;
	PacketData::Animation animation;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(entityId);
		_stream.Write(animation);
	}

	void Deserialize(NetworkStream& _stream) override {
		entityId = _stream.Read<EntityId>();
		animation = _stream.Read<PacketData::Animation>();
	}
};

// Used for simple actions, like sneaking
struct PlayerAction : BasePacket {
	PlayerAction() : BasePacket{ PacketId::PlayerAction } {}
	EntityId entityId;
	PacketData::PlayerAction action;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(entityId);
		_stream.Write(action);
	}

	void Deserialize(NetworkStream& _stream) override {
		entityId = _stream.Read<EntityId>();
		action = _stream.Read<PacketData::PlayerAction>();
	}
};

// Used for spawning other players in the world
struct SpawnPlayer : BasePacket {
	SpawnPlayer() : BasePacket{ PacketId::SpawnPlayer } {}
	EntityId entityId;
	std::string username;
	Int32_3 qPosition;
	union {
		Int8_2 qRotation;
		struct {
			int8_t qYaw, qPitch;
		};
	};
	ItemId heldItemId;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(entityId);
		_stream.WriteString16(username);
		_stream.Write(qPosition.x);
		_stream.Write(qPosition.y);
		_stream.Write(qPosition.z);
		_stream.Write(qYaw);
		_stream.Write(qPitch);
		_stream.Write(heldItemId);
	}

	void Deserialize(NetworkStream& _stream) override {
		entityId = _stream.Read<EntityId>();
		username = _stream.ReadString16();
		qPosition.x = _stream.Read<int32_t>();
		qPosition.y = _stream.Read<int32_t>();
		qPosition.z = _stream.Read<int32_t>();
		qYaw = _stream.Read<int8_t>();
		qPitch = _stream.Read<int8_t>();
		heldItemId = _stream.Read<ItemId>();
	}
};

// Used for spawning items in the world
struct SpawnItem : BasePacket {
	SpawnItem() : BasePacket{ PacketId::SpawnItem } {}
	EntityId entityId;
	ItemStack item;
	Int32_3 qPosition;
	Int8_3 qVelocity;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(entityId);
		_stream.Write(item.id);
		_stream.Write(item.count);
		_stream.Write(item.data);
		_stream.Write(qPosition.x);
		_stream.Write(qPosition.y);
		_stream.Write(qPosition.z);
		_stream.Write(qVelocity.x);
		_stream.Write(qVelocity.y);
		_stream.Write(qVelocity.z);
	}

	void Deserialize(NetworkStream& _stream) override {
		entityId = _stream.Read<EntityId>();
		item.id = _stream.Read<ItemId>();
		item.count = _stream.Read<ItemAmount>();
		item.data = _stream.Read<ItemDamage>();
		qPosition.x = _stream.Read<int32_t>();
		qPosition.y = _stream.Read<int32_t>();
		qPosition.z = _stream.Read<int32_t>();
		qVelocity.x = _stream.Read<int8_t>();
		qVelocity.y = _stream.Read<int8_t>();
		qVelocity.z = _stream.Read<int8_t>();
	}
};

// Used for collecting items visually
struct CollectItem : BasePacket {
	CollectItem() : BasePacket{ PacketId::CollectItem } {}
	EntityId itemEntityId;
	EntityId collectorEntityId;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(itemEntityId);
		_stream.Write(collectorEntityId);
	}

	void Deserialize(NetworkStream& _stream) override {
		itemEntityId = _stream.Read<EntityId>();
		collectorEntityId = _stream.Read<EntityId>();
	}
};

// Used for spawning objects in the world
struct SpawnObject : BasePacket {
	SpawnObject() : BasePacket{ PacketId::SpawnObject } {}
	EntityId entityId;
	PacketData::ObjectType objectType;
	Int32_3 qPosition;
	EntityId ownerEntityId = 0;
	Int16_3 qVelocity;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(entityId);
		_stream.Write(objectType);
		_stream.Write(qPosition.x);
		_stream.Write(qPosition.y);
		_stream.Write(qPosition.z);
		_stream.Write(ownerEntityId);
		if (ownerEntityId) {
			_stream.Write(qVelocity.x);
			_stream.Write(qVelocity.y);
			_stream.Write(qVelocity.z);
		}
	}

	void Deserialize(NetworkStream& _stream) override {
		entityId = _stream.Read<EntityId>();
		objectType = _stream.Read<PacketData::ObjectType>();
		qPosition.x = _stream.Read<int32_t>();
		qPosition.y = _stream.Read<int32_t>();
		qPosition.z = _stream.Read<int32_t>();
		ownerEntityId = _stream.Read<EntityId>();
		if (ownerEntityId) {
			qVelocity.x = _stream.Read<int16_t>();
			qVelocity.y = _stream.Read<int16_t>();
			qVelocity.z = _stream.Read<int16_t>();
		}
	}
};

// Used for spawning mobs in the world
struct SpawnMob : BasePacket {
	SpawnMob() : BasePacket{ PacketId::SpawnMob } {}
	EntityId entityId;
	PacketData::MobType mobType;
	Int32_3 qPosition;
	union {
		Int8_2 qRotation;
		struct {
			int8_t qYaw, qPitch;
		};
	};
	std::vector<PacketData::EntityMetadata::DataEntry> metadata;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(entityId);
		_stream.Write(mobType);
		_stream.Write(qPosition.x);
		_stream.Write(qPosition.y);
		_stream.Write(qPosition.z);
		_stream.Write(qYaw);
		_stream.Write(qPitch);
		_stream.WriteEntityMetadata(metadata);
	}

	void Deserialize(NetworkStream& _stream) override {
		entityId = _stream.Read<EntityId>();
		mobType = _stream.Read<PacketData::MobType>();
		qPosition.x = _stream.Read<int32_t>();
		qPosition.y = _stream.Read<int32_t>();
		qPosition.z = _stream.Read<int32_t>();
		qYaw = _stream.Read<int8_t>();
		qPitch = _stream.Read<int8_t>();
		_stream.ReadEntityMetadata(metadata);
	}
};

// Used for spawning paintings in the world
struct SpawnPainting : BasePacket {
	SpawnPainting() : BasePacket{ PacketId::SpawnPainting } {}
	EntityId entityId;
	std::string title;
	Int32_3 qPosition;
	PacketData::PaintingDirection direction;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(entityId);
		_stream.WriteString16(title);
		_stream.Write(qPosition.x);
		_stream.Write(qPosition.y);
		_stream.Write(qPosition.z);
		_stream.Write(direction);
	}

	void Deserialize(NetworkStream& _stream) override {
		entityId = _stream.Read<EntityId>();
		title = _stream.ReadString16();
		qPosition.x = _stream.Read<int32_t>();
		qPosition.y = _stream.Read<int32_t>();
		qPosition.z = _stream.Read<int32_t>();
		direction = _stream.Read<PacketData::PaintingDirection>();
	}
};

// Unused, but exists for sending raw player input to the client/server
struct PlayerInput : BasePacket {
	PlayerInput() : BasePacket{ PacketId::PlayerInput } {}
	union {
		Float2 direction;
		struct {
			float strafeDirection, forwardDirection;
		};
	};
	union {
		Float2 rotation;
		struct {
			float pitch, yaw;
		};
	};
	bool jumping;
	bool sneaking;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(strafeDirection);
		_stream.Write(forwardDirection);
		_stream.Write(pitch);
		_stream.Write(yaw);
		_stream.Write(jumping);
		_stream.Write(sneaking);
	}

	void Deserialize(NetworkStream& _stream) override {
		strafeDirection = _stream.Read<float>();
		forwardDirection = _stream.Read<float>();
		pitch = _stream.Read<float>();
		yaw = _stream.Read<float>();
		jumping = _stream.Read<bool>();
		sneaking = _stream.Read<bool>();
	}
};

// Used to update an entities velocity
struct EntityVelocity : BasePacket {
	EntityVelocity() : BasePacket{ PacketId::EntityVelocity } {}
	EntityId entityId;
	Int16_3 velocity;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(entityId);
		_stream.Write(velocity.x);
		_stream.Write(velocity.y);
		_stream.Write(velocity.z);
	}

	void Deserialize(NetworkStream& _stream) override {
		entityId = _stream.Read<EntityId>();
		velocity.x = _stream.Read<int16_t>();
		velocity.y = _stream.Read<int16_t>();
		velocity.z = _stream.Read<int16_t>();
	}
};

// Used to despawn an entity
struct DespawnEntity : BasePacket {
	DespawnEntity() : BasePacket{ PacketId::DespawnEntity } {}
	EntityId entityId;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(entityId);
	}

	void Deserialize(NetworkStream& _stream) override {
		entityId = _stream.Read<EntityId>();
	}
};

// Unused, Base Packet for entity movement packets
struct EntityMovement : BasePacket {
	EntityMovement() : BasePacket{ PacketId::EntityMovement } {}

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
	}

	void Deserialize(NetworkStream& /*_stream*/) override {}
};

// Used for setting an entitys relative position
struct EntityPosition : BasePacket {
	EntityPosition() : BasePacket{ PacketId::EntityPosition } {}
	EntityId entityId;
	Int8_3 qrPosition;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(entityId);
		_stream.Write(qrPosition.x);
		_stream.Write(qrPosition.y);
		_stream.Write(qrPosition.z);
	}

	void Deserialize(NetworkStream& _stream) override {
		entityId = _stream.Read<EntityId>();
		qrPosition.x = _stream.Read<int8_t>();
		qrPosition.y = _stream.Read<int8_t>();
		qrPosition.z = _stream.Read<int8_t>();
	}
};

// Used for setting an entitys rotation
struct EntityRotation : BasePacket {
	EntityRotation() : BasePacket{ PacketId::EntityRotation } {}
	EntityId entityId;
	union {
		Int8_2 qRotation;
		struct {
			int8_t qYaw, qPitch;
		};
	};

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(entityId);
		_stream.Write(qYaw);
		_stream.Write(qPitch);
	}

	void Deserialize(NetworkStream& _stream) override {
		entityId = _stream.Read<EntityId>();
		qYaw = _stream.Read<int8_t>();
		qPitch = _stream.Read<int8_t>();
	}
};

// Used for setting an entitys relative position and rotation
struct EntityPositionAndRotation : BasePacket {
	EntityPositionAndRotation() : BasePacket{ PacketId::EntityPositionAndRotation } {}
	EntityId entityId;
	Int8_3 qrPosition;
	union {
		Int8_2 qRotation;
		struct {
			int8_t qYaw, qPitch;
		};
	};

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(entityId);
		_stream.Write(qrPosition.x);
		_stream.Write(qrPosition.y);
		_stream.Write(qrPosition.z);
		_stream.Write(qYaw);
		_stream.Write(qPitch);
	}

	void Deserialize(NetworkStream& _stream) override {
		entityId = _stream.Read<EntityId>();
		qrPosition.x = _stream.Read<int8_t>();
		qrPosition.y = _stream.Read<int8_t>();
		qrPosition.z = _stream.Read<int8_t>();
		qYaw = _stream.Read<int8_t>();
		qPitch = _stream.Read<int8_t>();
	}
};

// Used for setting an entitys absolute position
struct TeleportEntity : BasePacket {
	TeleportEntity() : BasePacket{ PacketId::TeleportEntity } {}
	EntityId entityId;
	Int32_3 qPosition;
	union {
		Int8_2 qRotation;
		struct {
			int8_t qYaw, qPitch;
		};
	};

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(entityId);
		_stream.Write(qPosition.x);
		_stream.Write(qPosition.y);
		_stream.Write(qPosition.z);
		_stream.Write(qYaw);
		_stream.Write(qPitch);
	}

	void Deserialize(NetworkStream& _stream) override {
		entityId = _stream.Read<EntityId>();
		qPosition.x = _stream.Read<int32_t>();
		qPosition.y = _stream.Read<int32_t>();
		qPosition.z = _stream.Read<int32_t>();
		qYaw = _stream.Read<int8_t>();
		qPitch = _stream.Read<int8_t>();
	}
};

// Used for some entity animations
struct EntityEvent : BasePacket {
	EntityEvent() : BasePacket{ PacketId::EntityEvent } {}
	EntityId entityId;
	PacketData::EntityEvent action;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(entityId);
		_stream.Write(action);
	}

	void Deserialize(NetworkStream& _stream) override {
		entityId = _stream.Read<EntityId>();
		action = _stream.Read<PacketData::EntityEvent>();
	}
};

// Used for mounting and dismounting entities
struct AddPassenger : BasePacket {
	AddPassenger() : BasePacket{ PacketId::AddPassenger } {}
	EntityId passengerEntityId;
	EntityId vehicleEntityId;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(passengerEntityId);
		_stream.Write(vehicleEntityId);
	}

	void Deserialize(NetworkStream& _stream) override {
		passengerEntityId = _stream.Read<EntityId>();
		vehicleEntityId = _stream.Read<EntityId>();
	}
};

// Used for mounting and dismounting entities
struct EntityMetadata : BasePacket {
	EntityMetadata() : BasePacket{ PacketId::EntityMetadata } {}
	EntityId entityId;
	std::vector<PacketData::EntityMetadata::DataEntry> metadata;

	// TODO: Ideally this'd immediately read/write
	// the relevant data for the entity behind the ID,
	// but for now we'll just read it into the metadata vector

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(entityId);
		_stream.WriteEntityMetadata(metadata);
	}

	void Deserialize(NetworkStream& _stream) override {
		entityId = _stream.Read<EntityId>();
		_stream.ReadEntityMetadata(metadata);
	}
};

// Tells the client to allocate or free a chunk slot. Must be sent before ChunkData
struct SetChunkVisibility : BasePacket {
	SetChunkVisibility() : BasePacket{ PacketId::SetChunkVisibility } {}
	Int32_2 pos;
	bool visible;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(pos.x);
		_stream.Write(pos.z);
		_stream.Write(visible);
	}

	void Deserialize(NetworkStream& _stream) override {
		pos.x = _stream.Read<int32_t>();
		pos.z = _stream.Read<int32_t>();
		visible = _stream.Read<bool>();
	}
};

// Sends compressed chunk data; always preceded by SetChunkVisibility
struct ChunkData : BasePacket {
	ChunkData() : BasePacket{ PacketId::Chunk } {}
	SlimInt3<int16_t> pos{ 0, 0, 0 };
	TriNumber<uint8_t> size{ CHUNK_WIDTH - 1, CHUNK_HEIGHT - 1, CHUNK_WIDTH - 1 };
	std::vector<uint8_t> compressedData;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(pos.x);
		_stream.Write(pos.y);
		_stream.Write(pos.z);
		_stream.Write(size.x);
		_stream.Write(size.y);
		_stream.Write(size.z);
		_stream.Write(static_cast<int32_t>(compressedData.size()));
		_stream.WriteBytes(compressedData.data(), compressedData.size());
	}

	void Deserialize(NetworkStream& _stream) override {
		pos.x = _stream.Read<int32_t>();
		pos.y = _stream.Read<int16_t>();
		pos.z = _stream.Read<int32_t>();
		size.x = _stream.Read<uint8_t>();
		size.y = _stream.Read<uint8_t>();
		size.z = _stream.Read<uint8_t>();
		int32_t length = _stream.Read<int32_t>();
		compressedData.resize(static_cast<size_t>(length));
		_stream.ReadBytes(compressedData.data(), static_cast<size_t>(length));
	}
};

// Used to set multiple blocks in a small area
struct SetMultipleBlocks : BasePacket {
	SetMultipleBlocks() : BasePacket{ PacketId::SetMultipleBlocks } {}
	Int32_2 chunkPosition;
	int16_t numberOfBlocks;
	std::vector<int16_t> blockCoordinates;
	std::vector<BlockType> blockTypes;
	std::vector<int8_t> blockMetadata; // Nibbles

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(chunkPosition.x);
		_stream.Write(chunkPosition.z);
		_stream.Write(numberOfBlocks);
		for (int16_t i = 0; i < numberOfBlocks; i++)
			_stream.Write(blockCoordinates[static_cast<size_t>(i)]);
		for (int16_t i = 0; i < numberOfBlocks; i++)
			_stream.Write(blockTypes[static_cast<size_t>(i)]);
		for (int16_t i = 0; i < numberOfBlocks; i++)
			_stream.Write(blockMetadata[static_cast<size_t>(i)]);
	}

	void Deserialize(NetworkStream& _stream) override {
		chunkPosition.x = _stream.Read<int32_t>();
		chunkPosition.z = _stream.Read<int32_t>();
		numberOfBlocks = _stream.Read<int16_t>();
		blockCoordinates.resize(static_cast<size_t>(numberOfBlocks));
		blockTypes.resize(static_cast<size_t>(numberOfBlocks));
		blockMetadata.resize(static_cast<size_t>(numberOfBlocks));
		for (int16_t i = 0; i < numberOfBlocks; i++)
			blockCoordinates[static_cast<size_t>(i)] = _stream.Read<int16_t>();
		for (int16_t i = 0; i < numberOfBlocks; i++)
			blockTypes[static_cast<size_t>(i)] = _stream.Read<BlockType>();
		for (int16_t i = 0; i < numberOfBlocks; i++)
			blockMetadata[static_cast<size_t>(i)] = _stream.Read<int8_t>();
	}
};

// Used to set a singular block
struct SetBlock : BasePacket {
	SetBlock() : BasePacket{ PacketId::SetBlock } {}
	SlimInt3<int8_t> position;
	Block block;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(position.x);
		_stream.Write(position.y);
		_stream.Write(position.z);
		_stream.Write(block.type);
		_stream.Write(block.data);
	}

	void Deserialize(NetworkStream& _stream) override {
		position.x = _stream.Read<int32_t>();
		position.y = _stream.Read<int8_t>();
		position.z = _stream.Read<int32_t>();
		block.type = _stream.Read<BlockType>();
		block.data = _stream.Read<uint8_t>();
	}
};

// Used to set a singular block
struct BlockEvent : BasePacket {
	BlockEvent() : BasePacket{ PacketId::BlockEvent } {}
	SlimInt3<int16_t> position;
	int8_t instrumentState;
	int8_t pitchDirection;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(position.x);
		_stream.Write(position.y);
		_stream.Write(position.z);
		_stream.Write(instrumentState);
		_stream.Write(pitchDirection);
	}

	void Deserialize(NetworkStream& _stream) override {
		position.x = _stream.Read<int32_t>();
		position.y = _stream.Read<int16_t>();
		position.z = _stream.Read<int32_t>();
		instrumentState = _stream.Read<int8_t>();
		pitchDirection = _stream.Read<int8_t>();
	}

	PacketData::NoteInstrument Instrument() const {
		return static_cast<PacketData::NoteInstrument>(instrumentState);
	}

	PacketData::NotePitch Pitch() const {
		return static_cast<PacketData::NotePitch>(pitchDirection);
	}

	PacketData::PistonState State() const {
		return static_cast<PacketData::PistonState>(instrumentState);
	}

	PacketData::PistonDirection Direction() const {
		return static_cast<PacketData::PistonDirection>(pitchDirection);
	}
};

// Used for explosions
struct Explosion : BasePacket {
	Explosion() : BasePacket{ PacketId::Explosion } {}
	Vec3 position;
	float radius;
	int32_t numberOfDestroyedBlocks;
	std::vector<int8_t> destroyedBlocks;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(position.x);
		_stream.Write(position.y);
		_stream.Write(position.z);
		_stream.Write(radius);
		_stream.Write(static_cast<int32_t>(numberOfDestroyedBlocks));
		_stream.WriteBytes(reinterpret_cast<const uint8_t*>(destroyedBlocks.data()), destroyedBlocks.size());
	}

	void Deserialize(NetworkStream& _stream) override {
		position.x = _stream.Read<double>();
		position.y = _stream.Read<double>();
		position.z = _stream.Read<double>();
		radius = _stream.Read<float>();
		numberOfDestroyedBlocks = _stream.Read<int32_t>();
		destroyedBlocks.resize(static_cast<size_t>(numberOfDestroyedBlocks));
		_stream.ReadBytes(reinterpret_cast<uint8_t*>(destroyedBlocks.data()),
		                  static_cast<size_t>(numberOfDestroyedBlocks));
	}
};

// Used to trigger world events, such as sound effects
struct WorldEvent : BasePacket {
	WorldEvent() : BasePacket{ PacketId::WorldEvent } {}
	PacketData::WorldEvent eventType;
	SlimInt3<int8_t> position;
	int32_t data;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(eventType);
		_stream.Write(position.x);
		_stream.Write(position.y);
		_stream.Write(position.z);
		_stream.Write(data);
	}

	void Deserialize(NetworkStream& _stream) override {
		eventType = _stream.Read<PacketData::WorldEvent>();
		position.x = _stream.Read<int32_t>();
		position.y = _stream.Read<int8_t>();
		position.z = _stream.Read<int32_t>();
		data = _stream.Read<int32_t>();
	}
};

// Used to trigger global game events, such as rain
struct GameEvent : BasePacket {
	GameEvent() : BasePacket{ PacketId::GameEvent } {}
	PacketData::GameEvent eventId;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(eventId);
	}

	void Deserialize(NetworkStream& _stream) override {
		eventId = _stream.Read<PacketData::GameEvent>();
	}
};

// Used to spawn a lightning bolt
struct LightningBolt : BasePacket {
	LightningBolt() : BasePacket{ PacketId::LightningBolt } {}
	EntityId entityId;
	// This is only ever "1", which means lightning
	int8_t entityType = 1;
	Int32_3 position;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(entityId);
		_stream.Write(entityType);
		_stream.Write(position.x);
		_stream.Write(position.y);
		_stream.Write(position.z);
	}

	void Deserialize(NetworkStream& _stream) override {
		entityId = _stream.Read<EntityId>();
		entityType = _stream.Read<int8_t>();
		position.x = _stream.Read<int32_t>();
		position.y = _stream.Read<int32_t>();
		position.z = _stream.Read<int32_t>();
	}
};

// Used for signaling when a container is opened
struct OpenContainer : BasePacket {
	OpenContainer() : BasePacket{ PacketId::OpenContainer } {}
	WindowId windowId;
	PacketData::WindowType windowType;
	std::string title; // This is String8!!
	int8_t slotCount;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(windowId);
		_stream.Write(windowType);
		_stream.WriteString8(title);
		_stream.Write(slotCount);
	}
	void Deserialize(NetworkStream& _stream) override {
		windowId = _stream.Read<WindowId>();
		windowType = _stream.Read<PacketData::WindowType>();
		title = _stream.ReadString8();
		slotCount = _stream.Read<int8_t>();
	}
};

// Used for signaling when a container was closed
struct CloseContainer : BasePacket {
	CloseContainer() : BasePacket{ PacketId::CloseContainer } {}
	WindowId windowId;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(windowId);
	}
	void Deserialize(NetworkStream& _stream) override {
		windowId = _stream.Read<WindowId>();
	}
};

// Used for signaling when a slot was clicked
struct ClickSlot : BasePacket {
	ClickSlot() : BasePacket{ PacketId::ClickSlot } {}
	WindowId windowId;
	NetworkSlotId slotId;
	TransactionId transactionId;
	ItemStack item;
	bool rightClick;
	bool shift;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(windowId);
		_stream.Write(slotId);
		_stream.Write(rightClick);
		_stream.Write(transactionId);
		_stream.Write(shift);
		_stream.Write(item.id);
		if (item.id != Items::Id::INVALID) {
			_stream.Write(item.count);
			_stream.Write(item.data);
		}
	}
	void Deserialize(NetworkStream& _stream) override {
		windowId = _stream.Read<WindowId>();
		slotId = _stream.Read<NetworkSlotId>();
		rightClick = _stream.Read<bool>();
		transactionId = _stream.Read<TransactionId>();
		shift = _stream.Read<bool>();
		item.id = _stream.Read<ItemId>();
		if (item.id != Items::Id::INVALID) {
			item.count = _stream.Read<ItemAmount>();
			item.data = _stream.Read<ItemDamage>();
		}
	}
};

// Used for setting the contents of a slot
struct SetSlot : BasePacket {
	SetSlot() : BasePacket{ PacketId::SetSlot } {}
	WindowId windowId;
	NetworkSlotId slotId;
	ItemStack item;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(windowId);
		_stream.Write(slotId);
		_stream.Write(item.id);
		if (item.id != Items::Id::INVALID) {
			_stream.Write(item.count);
			_stream.Write(item.data);
		}
	}
	void Deserialize(NetworkStream& _stream) override {
		windowId = _stream.Read<WindowId>();
		slotId = _stream.Read<NetworkSlotId>();
		item.id = _stream.Read<ItemId>();
		if (item.id != Items::Id::INVALID) {
			item.count = _stream.Read<ItemAmount>();
			item.data = _stream.Read<ItemDamage>();
		}
	}
};

// Possibly we do this by passing in the whole inventory?
// Used for filling a container with data
struct FillContainer : BasePacket {
	FillContainer() : BasePacket{ PacketId::FillContainer } {}
	WindowId windowId;
	std::vector<ItemStack> items;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(windowId);
		_stream.Write(int16_t(items.size()));
		for (ItemStack item : items) {
			_stream.Write(item.id);
			if (item.id != Items::Id::INVALID) {
				_stream.Write(item.count);
				_stream.Write(item.data);
			}
		}
	}
	void Deserialize(NetworkStream& _stream) override {
		windowId = _stream.Read<WindowId>();
		size_t numberOfSlots = size_t(_stream.Read<int16_t>());
		items.resize(numberOfSlots, ItemStack{ Items::Id::INVALID });
		for (size_t i = 0; i < numberOfSlots; i++) {
			items[i].id = _stream.Read<ItemId>();
			if (items[i].id != Items::Id::INVALID) {
				items[i].count = _stream.Read<ItemAmount>();
				items[i].data = _stream.Read<ItemDamage>();
			}
		}
	}
};

// Used for setting data for containers, such as furnace progress
struct ContainerData : BasePacket {
	ContainerData() : BasePacket{ PacketId::ContainerData } {}
	WindowId windowId;
	struct {
		PacketData::ContainerDataType type;
		int16_t value;
	} containerData;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(windowId);
		_stream.Write(containerData.type);
		_stream.Write(containerData.value);
	}
	void Deserialize(NetworkStream& _stream) override {
		windowId = _stream.Read<WindowId>();
		containerData.type = _stream.Read<PacketData::ContainerDataType>();
		containerData.value = _stream.Read<int16_t>();
	}
};

// Used for checking if the performed transaction was valid and got through successfully
struct ContainerTransaction : BasePacket {
	ContainerTransaction() : BasePacket{ PacketId::ContainerTransaction } {}
	WindowId windowId;
	TransactionId transactionId;
	bool accepted;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(windowId);
		_stream.Write(transactionId);
		_stream.Write(accepted);
	}
	void Deserialize(NetworkStream& _stream) override {
		windowId = _stream.Read<WindowId>();
		transactionId = _stream.Read<TransactionId>();
		accepted = _stream.Read<bool>();
	}
};

// Use for updating the text on signs
struct UpdateSign : BasePacket {
	UpdateSign() : BasePacket{ PacketId::UpdateSign } {}
	SlimInt3<int16_t> position;
	std::string lines[4];

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(position.x);
		_stream.Write(position.y);
		_stream.Write(position.z);
		_stream.WriteString16(lines[0]);
		_stream.WriteString16(lines[1]);
		_stream.WriteString16(lines[2]);
		_stream.WriteString16(lines[3]);
	}

	void Deserialize(NetworkStream& _stream) override {
		position.x = _stream.Read<int32_t>();
		position.y = _stream.Read<int16_t>();
		position.z = _stream.Read<int32_t>();
		lines[0] = _stream.ReadString16();
		lines[1] = _stream.ReadString16();
		lines[2] = _stream.ReadString16();
		lines[3] = _stream.ReadString16();
	}
};

// Used for updating custom item data, only used by maps
struct ItemData : BasePacket {
	ItemData() : BasePacket{ PacketId::ItemData } {}
	ItemId itemId;
	MapId mapId;
	std::vector<uint8_t> data;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(itemId);
		_stream.Write(mapId);
		_stream.Write(uint8_t(data.size()));
		_stream.WriteBytes(data.data(), data.size());
	}

	void Deserialize(NetworkStream& _stream) override {
		itemId = _stream.Read<ItemId>();
		mapId = _stream.Read<MapId>();
		uint8_t size = _stream.Read<uint8_t>();
		data.resize(size_t(size));
		_stream.ReadBytes(data.data(), size_t(size));
	}
};

// Used for changing the value of a statistic
struct IncrementStatistic : BasePacket {
	IncrementStatistic() : BasePacket{ PacketId::IncrementStatistic } {}
	int32_t statisticId; // TODO: Replace with Enum
	int8_t amount;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.Write(statisticId);
		_stream.Write(amount);
	}

	void Deserialize(NetworkStream& _stream) override {
		statisticId = _stream.Read<int32_t>();
		amount = _stream.Read<int8_t>();
	}
};

// Used for disconnecting with a disconnect reason
struct Disconnect : BasePacket {
	Disconnect() : BasePacket{ PacketId::Disconnect } {}
	std::string reason;

	void Serialize(NetworkStream& _stream) const override {
		_stream.Write(id);
		_stream.WriteString16(reason);
	}

	void Deserialize(NetworkStream& _stream) override {
		reason = _stream.ReadString16();
	}
};
}; // namespace Packet