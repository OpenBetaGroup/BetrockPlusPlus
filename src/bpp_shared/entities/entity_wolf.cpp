/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#include "entity_wolf.h"
#include "entity_mob.h"

bool WolfEntity::IsTamed() {
	return !ownerUsername.empty();
}

void WolfEntity::Tick() {
	// TODO: TEMP HACK TO PREVENT WOLVES FORM WALKING WHEN SITTING!!!!! REPLACE!!!!!
	if (isSitting)
		ticksToFollowTarget = 999;
	MobEntity::Tick();
}

void WolfEntity::OnDeath(Entity* /*_killer*/, DamageType /*_type*/) {
}

void WolfEntity::EncodeMetadata(std::vector<PacketData::EntityMetadata::DataEntry>& _metadata) {
	Entity::EncodeMetadata(_metadata);

	const int8_t value = static_cast<int8_t>(isSitting | isAngry << 1 | IsTamed() << 2);

	_metadata.push_back({ .type = PacketData::EntityMetadata::BYTE, .index = 16, .value = value });
	_metadata.push_back({ .type = PacketData::EntityMetadata::STRING, .index = 17, .value = ownerUsername });
	_metadata.push_back({ .type = PacketData::EntityMetadata::INTEGER, .index = 18, .value = health });
}

bool WolfEntity::DecodeMetadata(const std::vector<PacketData::EntityMetadata::DataEntry>& _metadata) {
	if (!Entity::DecodeMetadata(_metadata)) {
		return false;
	}

    auto* metaFlags = FindMetadata<int8_t>(_metadata, PacketData::EntityMetadata::BYTE, 16);
    auto* metaOwnerUsername = FindMetadata<std::string>(_metadata, PacketData::EntityMetadata::STRING, 17);
    auto* metaHealth = FindMetadata<int32_t>(_metadata, PacketData::EntityMetadata::INTEGER, 18);
	if (metaFlags && metaOwnerUsername && metaHealth) {
		isSitting = (*metaFlags & 0b001) != 0;
		isAngry = (*metaFlags & 0b010) != 0;
		//isTamed = (*metaFlags & 0b100) != 0;
        ownerUsername = *metaOwnerUsername;
        health = *metaHealth;
		return true;
	}
	return false;
}

void WolfEntity::LoadFromNbt(Tag& _nbt) {
	AnimalEntity::LoadFromNbt(_nbt);
	isSitting = _nbt.Has("Sitting") ? _nbt.compound["Sitting"].GetByte() : false;
	isAngry = _nbt.Has("Angry") ? _nbt.compound["Angry"].GetByte() : false;
	ownerUsername = _nbt.Has("Owner") ? _nbt.compound["Owner"].GetString() : "";
}

std::optional<Tag> WolfEntity::SerializeToNbt() {
	auto tag = AnimalEntity::SerializeToNbt();
	if (!tag)
		return std::nullopt;

	Tag sittingTag;
	sittingTag.type = TAG_BYTE;
	sittingTag.name = "Sitting";
	sittingTag.byteValue = this->isSitting;

	Tag angryTag;
	angryTag.type = TAG_BYTE;
	angryTag.name = "Angry";
	angryTag.byteValue = this->isAngry;

	Tag ownerUsernameTag;
	ownerUsernameTag.type = TAG_STRING;
	ownerUsernameTag.name = "Owner";
	ownerUsernameTag.stringValue = this->ownerUsername;

	tag->compound["Sitting"] = sittingTag;
	tag->compound["Angry"] = angryTag;
	tag->compound["Owner"] = ownerUsernameTag;

	return tag;
}