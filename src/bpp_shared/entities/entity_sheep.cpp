/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#include "entity_sheep.h"
#include "blocks.h"
#include "entity_animal.h"
#include <string>
#include <unordered_map>
#include <variant>

void SheepEntity::LoadFromNbt(Tag& _nbt) {
	AnimalEntity::LoadFromNbt(_nbt);
	color = _nbt.Has("Color") ? _nbt.compound["Color"].GetByte() : 0;
	isSheared = _nbt.Has("Sheared") ? _nbt.compound["Sheared"].GetByte() : false;
}

std::optional<Tag> SheepEntity::SerializeToNbt() {
	auto tag = AnimalEntity::SerializeToNbt();
	if (!tag)
		return std::nullopt;
	Tag tagColor;
	tagColor.type = TAG_BYTE;
	tagColor.name = "Color";
	tagColor.byteValue = this->color;

	Tag sheared;
	sheared.type = TAG_BYTE;
	sheared.name = "Sheared";
	sheared.byteValue = this->isSheared;

	tag->compound["Color"] = tagColor;
	tag->compound["Sheared"] = sheared;

	return tag;
}

void SheepEntity::OnDeath(Entity* /*_killer*/, DamageType /*_type*/) {
	if (!isSheared)
		DropItemAtEntity(BLOCK_WOOL, 1, this->color);
}

void SheepEntity::EncodeMetadata(std::vector<PacketData::EntityMetadata::DataEntry>& _metadata) {
	Entity::EncodeMetadata(_metadata);

	const int8_t value = static_cast<int8_t>((color & 0x0F) | (isSheared ? 0x10 : 0));

	_metadata.push_back({ .type = PacketData::EntityMetadata::BYTE, .index = 16, .value = value });
}

bool SheepEntity::DecodeMetadata(const std::vector<PacketData::EntityMetadata::DataEntry>& _metadata) {
	if (!Entity::DecodeMetadata(_metadata)) {
		return false;
	}

	if (auto* raw = FindMetadata<int8_t>(_metadata, PacketData::EntityMetadata::BYTE, 16)) {
		isSheared = (*raw & 0x10) != 0;
		color = *raw & 0x0F;
		return true;
	}
	return false;
}
