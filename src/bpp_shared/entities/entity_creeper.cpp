/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#include "entity_creeper.h"
#include "entity_hostile.h"
#include "enums/items.h"
#include "java_random.h"
#include "world.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>

void CreeperEntity::OnDeath(Entity* _killer, DamageType /*_type*/) {
	// Drop gunpowder or a music disk if killed by a skeleton
	auto targetItem = Items::Id::GUNPOWDER;
	int itemCount = this->rand.NextInt(3);

	for (int i = 0; i < itemCount; i++) {
		DropItemAtEntity(targetItem, 1);
	}

	if (_killer && _killer->type == EntityType::SKELETON) {
		DropItemAtEntity(Items::Id::RECORD_13 + this->rand.NextInt(2), 1);
	}
}

void CreeperEntity::Tick() {
	lastActiveTime = timeSinceIgnited;

	HostileEntity::Tick();

	// If we no longer have anything to be igniting toward, count the fuse back down
	if (!attackTarget.lock() && timeSinceIgnited > 0) {
		UpdateMetadata<int8_t>(creeperState, -1);
		--timeSinceIgnited;
		if (timeSinceIgnited < 0) {
			timeSinceIgnited = 0;
		}
	}
}

void CreeperEntity::TryAttackEntity(Entity& /*_target*/, float _distance) {
	bool primed = creeperState > 0;
	if ((!primed && _distance < 3.0f) || (primed && _distance < 7.0f)) {
		if (timeSinceIgnited == 0) {
		}

		UpdateMetadata<int8_t>(creeperState, 1);
		++timeSinceIgnited;
		if (timeSinceIgnited >= 30) {
			world->DoExplosion(this, this->position, this->powered ? 6.0f : 3.0f, /*doFire=*/false);
			isDead = true;
			return;
		}

		hasAttacked = true;
	} else {
		UpdateMetadata<int8_t>(creeperState, -1);
		--timeSinceIgnited;
		if (timeSinceIgnited < 0) {
			timeSinceIgnited = 0;
		}
	}
}

void CreeperEntity::OnTargetLostSight(Entity& /*_target*/, float /*_distance*/) {
	// Defuse a step while we can't see the target.
	if (timeSinceIgnited > 0) {
		UpdateMetadata<int8_t>(creeperState, -1);
		--timeSinceIgnited;
		if (timeSinceIgnited < 0) {
			timeSinceIgnited = 0;
		}
	}
}

void CreeperEntity::EncodeMetadata(std::vector<PacketData::EntityMetadata::DataEntry>& _metadata) {
	Entity::EncodeMetadata(_metadata);

	_metadata.push_back({ .type = PacketData::EntityMetadata::BYTE, .index = 16, .value = creeperState });
	_metadata.push_back({ .type = PacketData::EntityMetadata::BYTE, .index = 17, .value = int8_t(powered ? 1 : 0) });
}

bool CreeperEntity::DecodeMetadata(const std::vector<PacketData::EntityMetadata::DataEntry>& _metadata) {
	if (!Entity::DecodeMetadata(_metadata)) {
		return false;
	}

	bool found = false;
	if (auto* raw = FindMetadata<int8_t>(_metadata, PacketData::EntityMetadata::BYTE, 16)) {
		creeperState = *raw;
		found = true;
	}
	if (auto* raw = FindMetadata<int8_t>(_metadata, PacketData::EntityMetadata::BYTE, 17)) {
		powered = (*raw == 1);
		found = true;
	}
	return found;
}

std::optional<Tag> CreeperEntity::SerializeToNbt() {
	auto tag = HostileEntity::SerializeToNbt();
	if (!tag) {
		return std::nullopt;
	}

	if (powered) {
		Tag poweredTag;
		poweredTag.name = "powered";
		poweredTag.type = TAG_BYTE;
		poweredTag.byteValue = 1;
		tag->compound["powered"] = poweredTag;
	}

	return tag;
}

void CreeperEntity::LoadFromNbt(Tag& _nbt) {
	HostileEntity::LoadFromNbt(_nbt);
	powered = _nbt.Has("powered") && _nbt.compound["powered"].GetByte() != 0;
}