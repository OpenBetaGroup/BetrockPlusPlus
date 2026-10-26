/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#include "entity_wolf.h"
#include "entity_mob.h"
#include "entity_player.h"

bool WolfEntity::TryDespawn() {
	if (IsTamed())
		return false;
	return MobEntity::TryDespawn();
}

void WolfEntity::OnPlayerInteract(PlayerEntity* _entity) {

}

bool WolfEntity::AttackEntityFrom(Entity* _entity, int _damage, DamageType _type) {
	this->SetWolfSitting(false);

	auto attackerWasPlayer = dynamic_cast<PlayerEntity*>(_entity);
	auto isMobileEntity = dynamic_cast<MobileEntity*>(_entity);

	if (_entity && !attackerWasPlayer) {
		_damage = (_damage + 1) / 2;
	}

	if (!MobEntity::AttackEntityFrom(_entity, _damage, _type)) {
		return false;
	} else {
		if (!IsTamed() && !isAngry) {
			if (attackerWasPlayer) {
				this->SetWolfAngry(true);
				this->attackTarget = entityManager->GetEntityByIdShared(_entity->id);
			}

			if (isMobileEntity) {
				auto searchCollider = AABB{position.x, position.y, position.z, position.x + 1.0, position.y + 1.0, position.z + 1.0};
				searchCollider = searchCollider.Expand(16.0, 4.0, 16.0);

				auto candidates = entityManager->GetEntitiesWithinAabbOfType(searchCollider, this->type);
				for (auto& wolf : candidates) {
					auto wolfEntity = dynamic_cast<WolfEntity*>(wolf);
					if (!wolfEntity)
						continue;
					if (!wolfEntity->IsTamed() && wolfEntity->attackTarget.lock() == nullptr) {
						wolfEntity->attackTarget = entityManager->GetEntityByIdShared(_entity->id);
						if (attackerWasPlayer)
							wolfEntity->SetWolfAngry(true);
					}
				}
			}
		} else if (_entity && _entity != this) {
			// Annoying!
			std::string playerName = attackerWasPlayer ? attackerWasPlayer->username : "";
			std::transform(ownerUsername.begin(), ownerUsername.end(), ownerUsername.begin(),
			               [](unsigned char c) { return std::tolower(c); });
			std::transform(playerName.begin(), playerName.end(), playerName.begin(),
			               [](unsigned char c) { return std::tolower(c); });
			if (this->IsTamed() && attackerWasPlayer && playerName == ownerUsername) {
				return true;
			}
			attackTarget = entityManager->GetEntityByIdShared(_entity->id);
		}
	}
}

std::shared_ptr<Entity> WolfEntity::FindPlayerToAttack() {
	if (!this->isAngry)
		return nullptr;
	auto candidate = entityManager->GetClosestPlayerWithin(position, 16);
	return candidate;
}

void WolfEntity::TryAttackEntity(Entity& _target, float _distance) {
	auto activeTarget = attackTarget.lock();
	if (!activeTarget)
		return;
	if (_distance > 2.0f && _distance < 6.0f && rand.NextInt(10) == 0) {
		if (this->onGround) {
			double dx = _target.position.x - position.x;
			double dz = _target.position.z - position.z;
			float dist = std::sqrt(dx * dx + dz * dz);
			this->velocity.x = dx / dist * 0.5f * 0.8 + this->velocity.x * 0.2;
			this->velocity.z = dz / dist * 0.5f * 0.8 + this->velocity.z * 0.2;
			this->velocity.y = 0.4;
			return;
		}
	}
	if (attackTime <= 0 && _distance < 2.0f && _target.collider.maxY > collider.minY &&
	    _target.collider.minY < collider.maxY) {
		attackTime = 20;
		_target.AttackEntityFrom(this, GetAttackStrength(), DamageType::Punch);
	}
}

void WolfEntity::PathToOrTPToOwner(PlayerEntity& _owner, float _distance) {
	Int3 goal = { MathHelper::FloorDouble(_owner.position.x), MathHelper::FloorDouble(_owner.position.y),
		          MathHelper::FloorDouble(_owner.position.z) };
	SetGoal(goal);

	bool hasPath = !currentPath.empty();

	if (!hasPath && _distance > 12.0f) {
		int32_t baseX = goal.x - 2;
		int32_t baseY = goal.y;
		int32_t baseZ = goal.z - 2;

		for (int x = 0; x <= 4; x++) {
			for (int z = 0; z <= 4; z++) {
				// Only the outer ring
				if (x >= 1 && x <= 3 && z >= 1 && z <= 3)
					continue;

				Int3 feet = { baseX + x, baseY, baseZ + z };

				// Solid floor and two blocks of clear space above it
				if (world->IsBlockNormalCube({ feet.x, feet.y - 1, feet.z }) && !world->IsBlockNormalCube(feet) &&
				    !world->IsBlockNormalCube({ feet.x, feet.y + 1, feet.z })) {
					Teleport({ feet.x + 0.5, double(feet.y), feet.z + 0.5 }, { rotationYaw, rotationPitch });
					return;
				}
			}
		}
	}
}

void WolfEntity::UpdateAIState() {
	if (!EntityAlive())
		return;

	// Only a couple mobs use this!
	hasAttacked = IsMovementBlocked();

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
			FaceEntity(dynamic_cast<MobileEntity&>(*currentTarget), isSitting ? 20.0f : 40.0f,
			           isSitting ? 20.0f : 40.0f);
	} else {
		UpdateState();
	}

	randomYawVelocity *= 0.9f;

	if (!hasAttacked && !hasPath && IsTamed() && !passenger.lock()) {
		// Follow our owner
		auto owner = entityManager->GetPlayerWithName(ownerUsername);
		if (owner) {
			float distance = this->position.Distance(owner->position);
			if (distance > 5.0f) {
				PathToOrTPToOwner(*owner, distance);
			}
		} else {
			this->SetWolfSitting(true);
		}
	} else if (!attackTarget.lock() && !hasPath && !IsTamed() && rand.NextInt(100) == 0) {
		// Randomly attack sheep
		auto searchCollider = AABB{ position.x,       position.y,       position.z,
			                        position.x + 1.0, position.y + 1.0, position.z + 1.0 };
		searchCollider = searchCollider.Expand(16.0, 4.0, 16.0);

		EntityType sheepType = EntityType::SHEEP;
		auto candidates = entityManager->GetEntitiesWithinAabbOfType(searchCollider, sheepType);
		auto chosen = candidates[rand.NextInt(candidates.size())];
		attackTarget = entityManager->GetEntityByIdShared(chosen->id);
	}

	if (this->inWater)
		this->SetWolfSitting(false);

	// So health updates
	if (previousSentHealth != health) {
		previousSentHealth = health;
		wasMetadataUpdated = true;
	}
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