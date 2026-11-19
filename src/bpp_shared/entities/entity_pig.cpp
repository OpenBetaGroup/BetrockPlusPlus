/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#include "entity_pig.h"
#include "entity_animal.h"
#include "entity_mob.h"
#include "entity_player.h"
#include "enums/items.h"
#include "java_random.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>

bool PigEntity::TryDespawn() {
	if (isSaddled)
		return false;
	return MobEntity::TryDespawn();
}

void PigEntity::OnDeath(Entity* /*_killer*/, DamageType /*_type*/) {
	// Drop 0-2 porkchops, cooked if the pig was on fire
	auto targetItem = this->fireTicks > 0 ? Items::Id::PORKCHOP_COOKED : Items::Id::PORKCHOP;
	int itemCount = this->rand.NextInt(3);

	for (int i = 0; i < itemCount; i++) {
		DropItemAtEntity(targetItem, 1);
	}

	if (isSaddled)
		DropItemAtEntity(Items::SADDLE, 1);
}

void PigEntity::OnPlayerInteract(PlayerEntity* _entity) {
	if (!_entity)
		return;

	if (!isSaddled)
		return;

	auto rider = passenger.lock();
	if (rider && rider.get() != _entity) {
		// Someone else is already riding
		return;
	}

	if (_entity == rider.get()) {
		_entity->UnmountEntity();
		return;
	}

	auto selfPtr = entityManager->GetEntityByIdShared(this->id);
	if (selfPtr)
		_entity->MountEntity(selfPtr);
}

void PigEntity::EncodeMetadata(std::vector<PacketData::EntityMetadata::DataEntry>& _metadata) {
	Entity::EncodeMetadata(_metadata);

	const int8_t value = static_cast<int8_t>(isSaddled);

	_metadata.push_back({ .type = PacketData::EntityMetadata::BYTE, .index = 16, .value = value });
}

bool PigEntity::DecodeMetadata(const std::vector<PacketData::EntityMetadata::DataEntry>& _metadata) {
	if (!Entity::DecodeMetadata(_metadata)) {
		return false;
	}

	if (auto* raw = FindMetadata<int8_t>(_metadata, PacketData::EntityMetadata::BYTE, 16)) {
		isSaddled = *raw != 0;
		return true;
	}
	return false;
}

void PigEntity::LoadFromNbt(Tag& _nbt) {
	AnimalEntity::LoadFromNbt(_nbt);
	isSaddled = _nbt.Has("Saddle") ? _nbt.compound["Saddle"].GetByte() : false;
}

std::optional<Tag> PigEntity::SerializeToNbt() {
	auto tag = AnimalEntity::SerializeToNbt();
	if (!tag)
		return std::nullopt;

	Tag saddled;
	saddled.type = TAG_BYTE;
	saddled.name = "Saddle";
	saddled.byteValue = this->isSaddled;

	tag->compound["Saddle"] = saddled;

	return tag;
}