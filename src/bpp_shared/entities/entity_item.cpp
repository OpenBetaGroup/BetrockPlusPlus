/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#include "entity_item.h"
#include "entity_player.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>

void ItemEntity::LoadFromNbt(Tag& _nbt) {
	Entity::LoadFromNbt(_nbt);

	// Load item specific stuff
	this->health = _nbt.compound["Health"].GetShort();
	this->ticksExisted = _nbt.compound["Age"].GetShort();

	// Load our item
	const auto& itemCompound = _nbt.compound["Item"].GetCompound();
	this->itemStack = { itemCompound.at("id").GetShort(), itemCompound.at("Count").GetByte(),
		                itemCompound.at("Damage").GetShort() };
}

std::optional<Tag> ItemEntity::SerializeToNbt() {
	auto tag = Entity::SerializeToNbt();
	if (!tag)
		return std::nullopt;

	// Our additions
	Tag healthTag;
	healthTag.name = "Health";
	healthTag.type = TAG_SHORT;
	healthTag.shortValue = this->health;
	Tag ageTag;
	ageTag.name = "Age";
	ageTag.type = TAG_SHORT;
	ageTag.shortValue = this->ticksExisted;

	// Construct the item nbt
	Tag itemTag;
	itemTag.name = "Item";
	itemTag.type = TAG_COMPOUND;
	Tag idTag;
	idTag.name = "id";
	idTag.type = TAG_SHORT;
	idTag.shortValue = this->itemStack.id;
	Tag countTag;
	countTag.name = "Count";
	countTag.type = TAG_BYTE;
	countTag.byteValue = this->itemStack.count;
	Tag damageTag;
	damageTag.name = "Damage";
	damageTag.type = TAG_SHORT;
	damageTag.shortValue = this->itemStack.data;
	itemTag.compound["id"] = idTag;
	itemTag.compound["Count"] = countTag;
	itemTag.compound["Damage"] = damageTag;

	// Add our additions to the base tag
	tag->compound["Health"] = healthTag;
	tag->compound["Age"] = ageTag;
	tag->compound["Item"] = itemTag;

	return tag;
}

void ItemEntity::OnCollideWithPlayer(PlayerEntity& _entity) {
	if (pickupCooldown)
		return;
	auto beforeCount = itemStack.count;
	_entity.PickupItem(this->itemStack, this->id);
	if (beforeCount == itemStack.count)
		return;

	if (this->itemStack.count > 0) {
		// Spawn a new partial item
		std::shared_ptr<ItemEntity> item = std::make_shared<ItemEntity>(this->position);
		item->itemStack = this->itemStack;
		item->pickupCooldown = this->pickupCooldown;
		item->health = this->health;
		item->velocity = this->velocity;
		item->ticksExisted = this->ticksExisted;
		this->world->entityManager.AddEntity(item);
	}
	itemStack = {};
	this->isDead = true;
}

void ItemEntity::UpdateFallState(float /*_movedY*/) {
	fallDistance = 0;
}

void ItemEntity::Tick() {
	// Item entities have differing physics
	Entity::Tick();
	pickupCooldown--;
	pickupCooldown = std::max(int8_t(0), pickupCooldown);

	velocity.y -= 0.04;

	if (world->GetMaterial({ MathHelper::FloorDouble(position.x), MathHelper::FloorDouble(position.y),
	                         MathHelper::FloorDouble(position.z) }) == Material::Lava()) {
		velocity.y = 0.2;
		velocity.x = double((rand.NextFloat() - rand.NextFloat()) * 0.2F);
		velocity.z = double((rand.NextFloat() - rand.NextFloat()) * 0.2F);
	}

	PushOutOfBlocks({ position.x, (collider.minY + collider.maxY) / 2.0, position.z });
	Move(this->velocity);

	float horizontalDrag = 0.98f;
	if (onGround) {
		horizontalDrag = 0.58800006f;

		// Look up the block below us
		int bx = MathHelper::FloorDouble(position.x);
		int by = MathHelper::FloorDouble(collider.minY) - 1;
		int bz = MathHelper::FloorDouble(position.z);
		auto blockId = world->GetBlockId({ bx, by, bz });
		belowBlock = Blocks::blockProperties[blockId];

		if (blockId > BLOCK_AIR)
			horizontalDrag = belowBlock.slipperiness * 0.98f;
	}

	velocity.x *= double(horizontalDrag);
	velocity.y *= 0.9800000190734863;
	velocity.z *= double(horizontalDrag);

	// Bounce when we land
	if (onGround)
		velocity.y *= -0.5;

	if (ticksExisted >= 6000)
		isDead = true;
}