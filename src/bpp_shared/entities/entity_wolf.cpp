/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#include "entity_wolf.h"
#include "entity_mob.h"

void WolfEntity::OnPlayerInteract(PlayerEntity* _entity) {

}

bool WolfEntity::AttackEntityFrom(Entity* _entity, int _damage, DamageType _type) {
	return MobEntity::AttackEntityFrom(_entity, _damage, _type);
}

std::shared_ptr<Entity> WolfEntity::FindPlayerToAttack() {
	if (!this->isAngry)
		return nullptr;
	auto candidate = entityManager->GetClosestPlayerWithin(position, 16);
	return candidate;
}

void WolfEntity::TryAttackEntity(Entity& _target, float _distance) {
	// Check if our attack time and distance are valid, make sure our Y axis overlap
	if (attackTime <= 0 && _distance < 2.0f && _target.collider.maxY > collider.minY &&
	    _target.collider.minY < collider.maxY) {
		attackTime = 20;
		_target.AttackEntityFrom(this, GetAttackStrength(), DamageType::Punch);
	}
}

void WolfEntity::UpdateAIState() {
	if (!EntityAlive())
		return;

	// Only a couple mobs use this!
	hasAttacked = false;

	auto currentTarget = attackTarget.lock();

	// Get our target
	if (!currentTarget) {
		auto found = FindPlayerToAttack();
		if (found) {
			attackTarget = found;
			currentTarget = found;
			Int3 goal = { MathHelper::FloorDouble(found->position.x), MathHelper::FloorDouble(found->position.y),
				          MathHelper::FloorDouble(found->position.z) };
			SetGoal(goal);
		}
	} else if (currentTarget->isDead) {
		attackTarget.reset();
		currentTarget = nullptr;
	} else {
		float distance = float(position.Distance(currentTarget->position));
		Vec3 eyeFrom = { position.x, position.y + GetEyeHeight(), position.z };
		Vec3 eyeTo = { currentTarget->position.x, currentTarget->position.y + currentTarget->height * 0.85f,
			           currentTarget->position.z };
		if (HasLineOfSight(eyeFrom, eyeTo))
			TryAttackEntity(*currentTarget, distance);
		else
			OnTargetLostSight(*currentTarget, distance);
	}

	// Decide whether to repath
	bool hasPath = !currentPath.empty();
	if (hasAttacked || !currentTarget || (hasPath && rand.NextInt(20) != 0)) {
		if (!hasAttacked)
			Wander();
	} else {
		Int3 goal = { MathHelper::FloorDouble(currentTarget->position.x),
			          MathHelper::FloorDouble(currentTarget->position.y),
			          MathHelper::FloorDouble(currentTarget->position.z) };
		SetGoal(goal);
	}

	rotationPitch = 0.0;

	// Follow our path and strafe if we are close and have attacked
	if (FollowPath()) {
		if (hasAttacked && currentTarget) {
			double dx = currentTarget->position.x - position.x;
			double dz = currentTarget->position.z - position.z;
			float previousYaw = rotationYaw;
			rotationYaw = float(std::atan2(dz, dx) * 180.0 / JavaMath::PI) - 90.0f;
			float angleDiff = (previousYaw - rotationYaw + 90.0f) * (JavaMath::PI / 180.0f);
			float forward = input.y;
			input.x = -std::sin(angleDiff) * forward;
			input.y = std::cos(angleDiff) * forward;
		}

		if (currentTarget)
			FaceEntity(dynamic_cast<MobileEntity&>(*currentTarget), 30.0f, 30.0f);
	} else {
		UpdateState(); // Idle if we dont have a path
	}

	randomYawVelocity *= 0.9f;
}

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