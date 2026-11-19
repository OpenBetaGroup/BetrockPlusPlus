/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#include "entity_tnt.h"
#include "entity.h"
#include <cstdint>
#include <string>
#include <unordered_map>

static Tag SetByte(std::string _name, int8_t _value) {
	Tag byteTag;
	byteTag.type = TAG_BYTE;
	byteTag.name = _name;
	byteTag.byteValue = _value;

	return byteTag;
}

void TntEntity::Tick() {
	if (!world)
		return;

	this->velocity.y -= double(0.04f);
	this->Move(this->velocity);
	this->velocity *= double(0.98f);
	if (this->onGround) {
		this->velocity.x *= double(0.7f);
		this->velocity.y *= -0.5;
		this->velocity.z *= double(0.7f);
	}

	if (this->fuse-- <= 0) {
		this->isDead = true;
		this->Explode();
	}
}

std::optional<Tag> TntEntity::SerializeToNbt() {
	auto tag = Entity::SerializeToNbt();
	if (!tag)
		return std::nullopt;

	tag->compound["Fuse"] = SetByte("Fuse", this->fuse);

	return tag;
}

void TntEntity::LoadFromNbt(Tag& _nbt) {
	Entity::LoadFromNbt(_nbt);

	this->fuse = _nbt.Get("Fuse").byteValue;
}