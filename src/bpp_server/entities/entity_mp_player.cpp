/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, jomner <github.com/jomner>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#include "entity_mp_player.h"
#include "../player_conn/player_session.h"
#include "entities/entity_item.h"
#include "entities/entity_player.h"
#include "inventory/item_stack.h"
#include "networking/network_stream.h"
#include "networking/packets.h"

void EntityMPPlayer::UsePortal() {
	if (!this->session || !this->session->server)
		return;

	switch(this->dim) {
		case Dimension::Overworld:
			if (!Gamerule::Get<bool>(Gamerule::Id::AllowEnteringNetherUsingPortals))
				return;
			this->session->server->SendPlayerToDimension(Dimension::Nether, *this->session);
			break;
		case Dimension::Nether:
			this->session->server->SendPlayerToDimension(Dimension::Overworld, *this->session);
			break;
		default:
			return;
	}
}

SleepFailureReason EntityMPPlayer::TrySleep(Int3 _pos) {
	if (!this->session)
		return SleepFailureReason::OTHER;

	auto result = PlayerEntity::TrySleep(_pos);
	if (result != SleepFailureReason::SUCCESS)
		return result;

	Packet::InteractWithBlock pkt;
	pkt.entityId = session->entity->id;
	pkt.interactionId = PacketData::BlockInteraction::SLEEPING;
	pkt.position = { _pos.x, static_cast<int8_t>(_pos.y), _pos.z };
	pkt.Serialize(session->stream);

	Packet::Animation anim;
	anim.entityId = session->entity->id;
	anim.animation = PacketData::Animation::PUNCH;
	session->entityTracker->SendPacketToViewers(anim, this->id);
	session->entityTracker->SendPacketToViewers(pkt, this->id);

	this->Teleport(this->position, { rotationYaw, rotationPitch });

	return SleepFailureReason::SUCCESS;
}

void EntityMPPlayer::WakeUp(bool _confirmSpawn) {
	if (!this->session)
		return;

	PlayerEntity::WakeUp(_confirmSpawn);

	if (_confirmSpawn) {
		session->hasBedSpawn = true;
		session->spawnPosition = this->bedPosition.WithOffset(Cardinal::Value::Up);
	}

	Packet::Animation anim;
	anim.entityId = this->id;
	anim.animation = PacketData::Animation::LEAVE_BED;
	anim.Serialize(session->stream);
	session->entityTracker->SendPacketToViewers(anim, this->id);

	this->Teleport(this->position, { rotationYaw, rotationPitch });
	session->position.pos = this->position;
	session->pendingTeleport = this->position;
	Packet::PlayerPositionAndRotation pos;
	pos.rotation = session->rotation;
	pos.onGround = false;
	pos.position = { this->position.x, this->position.y + PLAYER_EYE_HEIGHT, this->position.z };
	pos.cameraY = this->position.y; // This is backwards, thanks notch
	pos.Serialize(session->stream);
}

void EntityMPPlayer::OnMountEntity() {
	auto vehiclePtr = this->vehicle.lock().get();
	if (this->session->entityTracker && vehiclePtr) {
		Packet::AddPassenger pkt;
		pkt.passengerEntityId = this->id;
		pkt.vehicleEntityId = vehiclePtr->id;
		this->session->entityTracker->SendPacketToViewers(pkt, this->id);
		pkt.Serialize(session->stream);
	}
}

void EntityMPPlayer::OnDismountEntity() {
	if (this->session)
		this->session->position.pos = this->position;
	if (this->session->entityTracker) {
		Packet::AddPassenger pkt;
		pkt.passengerEntityId = this->id;
		pkt.vehicleEntityId = -1;
		this->session->entityTracker->SendPacketToViewers(pkt, this->id);
		pkt.Serialize(session->stream);
	}
}

bool EntityMPPlayer::DropItem(ItemStack _stack) {
	if (_stack.id == Items::Id::INVALID || _stack.count <= 0)
		return false;

	// Create the item entity
	Vec3 itemPos = { position.x, position.y - 0.3 + PLAYER_EYE_HEIGHT, position.z };
	std::shared_ptr<ItemEntity> itemEntity = std::make_shared<ItemEntity>(itemPos);
	itemEntity->itemStack = _stack;
	itemEntity->pickupCooldown = 40; // So we don't pick it up instantly

	// Give ourselves some random velocity based on look direction
	float initVelocity = 0.3f;
	itemEntity->velocity.x = double(-std::sin(this->rotationYaw / 180.0F * JavaMath::PI_FLOAT) *
	                                std::cos(this->rotationPitch / 180.0F * JavaMath::PI_FLOAT) * initVelocity);
	itemEntity->velocity.z = double(std::cos(this->rotationYaw / 180.0F * JavaMath::PI_FLOAT) *
	                                std::cos(this->rotationPitch / 180.0F * JavaMath::PI_FLOAT) * initVelocity);
	itemEntity->velocity.y = double(-std::sin(this->rotationPitch / 180.0F * JavaMath::PI_FLOAT) * initVelocity + 0.1F);

	// Add a little bit of randomness
	initVelocity = 0.02f;
	float angle = rand.NextFloat() * JavaMath::PI_FLOAT * 2.0f;
	initVelocity *= rand.NextFloat();
	itemEntity->velocity.x += double(std::cos(angle) * initVelocity);
	itemEntity->velocity.y += double((rand.NextFloat() - rand.NextFloat()) * 0.1f);
	itemEntity->velocity.z += double(std::sin(angle) * initVelocity);

	// Register our item with the world
	this->world->entityManager.AddEntity(std::move(itemEntity));
	return true;
}

bool EntityMPPlayer::PickupItem(ItemStack& _stack, EntityId _entityId) {
	if (this->isFirstUpdate)
		return false;

	if (!this->EntityAlive())
		return false;

	if (this->session->inventory.PickupItem(_stack)) {
		Packet::CollectItem pkt;
		pkt.collectorEntityId = this->id;
		pkt.itemEntityId = _entityId;
		if (this->session->entityTracker)
			this->session->entityTracker->SendPacketToViewers(pkt, this->id);
		pkt.Serialize(this->session->stream);
		return true;
	}

	return false;
}

static constexpr double CLEAR_CHECK_TOLERANCE = 0.05;
static constexpr double ROLLBACK_NUDGE = 0.06;
static constexpr int32_t TELEPORT_RESEND_TICKS = 20;

void EntityMPPlayer::HandlePositionChecks() {
	if (isDead || !world)
		return;

	if (!Gamerule::Get<bool>(Gamerule::Id::PlayerMovementCheck))
		return;

	this->velocity = {};
	if (this->vehicle.lock().get()) {
		// Vehicles are weird!
		if (!session->pendingPosition)
			return;
		Vec3 pos = *session->pendingPosition;
		session->pendingPosition.reset();
		if (pos.x <= -1 || pos.x > 1 || pos.z <= -1 || pos.z > 1)
			return;
		this->velocity = pos;
		this->velocity.y = 0;

		// Leave the vehicle on teleport
		if (session->pendingTeleport) {
			this->UnmountEntity();
			return;
		}

		// Update our position to the session
		if (!this->session)
			return;
		this->session->position.pos = this->position;
		return;
	}

	// We have a pending teleport. Check to see if the player caught up
	if (session->pendingTeleport && session->pendingPosition) {
		if (this->isSleeping) {
			this->WakeUp();
		}
		// Reset fall state
		fallDistance = 0;
		onGround = true;
		Vec3 delta = *session->pendingPosition - *session->pendingTeleport;
		auto dist = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
		const int checkRange = 32;

		if (dist > 0.0625 || !world->AABBinValidChunks(this->collider.Expand(checkRange, checkRange, checkRange))) {
			session->pendingPosition.reset();
			if (++ticksSinceTeleportSent < TELEPORT_RESEND_TICKS)
				return;
			ticksSinceTeleportSent = 0;
			// Still not there, so send another tp packet and reset our position
			this->Teleport(*session->pendingTeleport, { rotationYaw, rotationPitch });
			session->position.pos = *session->pendingTeleport;
			Packet::PlayerPositionAndRotation pkt;
			pkt.rotation = session->rotation; // what the client last told us, so they keep facing the same way
			pkt.onGround = onGround;
			pkt.position = { position.x, position.y + PLAYER_EYE_HEIGHT, position.z };
			pkt.cameraY = position.y; // This is backwards, thanks notch
			pkt.Serialize(session->stream);
			return;
		}
		// Client acknowledged our tp
		session->pendingTeleport.reset();
		this->fallDistance = 0;
		this->simulatedFallDistance = 0;
	}

	if (this->isSleeping) {
		if (session->pendingPosition) {
			session->position.pos = this->position;
			session->pendingPosition.reset();
		}
		return;
	}

	// If we recieved a movement packet this Tick do our server side checks
	if (session->pendingPosition) {
		Vec3 claimed = *session->pendingPosition;

		// This means the client is still trying to tell us we are in a vehicle
		if (claimed.y == -999)
			return;

		// Re-simulate our move
		bool savedOnGround = onGround;
		bool residualTooLarge = false;
		bool movedWrong = false;
		bool wasClearBefore = world
		                          ->GetCollidingBoundingBoxes(collider.Expand(-CLEAR_CHECK_TOLERANCE,
		                                                                      -CLEAR_CHECK_TOLERANCE,
		                                                                      -CLEAR_CHECK_TOLERANCE),
		                                                      /*_mover=*/nullptr)
		                          .empty();
		Vec3 lastPosition = this->position;
		Vec3 delta = claimed - lastPosition;

		// How far the client claims to have moved this tick
		double claimedTravelDistSq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
		if (claimedTravelDistSq > 100.0) {
			// Impossible distance for one tick. Don't simulate it (Move() sweeps the whole
			// path, so this would be a cheap way to stall the server), just roll the player back.
			movedWrong = true;
			delta = {};
		} else {
			Move(delta);
		}
		movedThisTick = true;

		// Simulate our own fall distance / on ground
		// Anti cheat is fun :p
		if (inWater || onGround || onLadder()) {
			simulatedFallDistance = 0;
		} else if (delta.y < 0) {
			simulatedFallDistance -= delta.y;
		}

		// Ignore if something is resetting our fall distance to 0
		// like ladders, water, etc
		if (simulatedFallDistance != 0) {
			const int verticalMax = 4.0;

			// Moved too fast down in one tick
			if (delta.y < -verticalMax)
				movedWrong = true;

			// Went up too far while nothing could be pushing us up
			if (delta.y > 0) {
				ticksInAir++;
				accumulatedUpDistance += delta.y;
				if (accumulatedUpDistance > verticalMax) {
					lastPosition = firstUpPosition;
					movedWrong = true;
				}
				if (ticksInAir == 1 && delta.y > 0.5) {
					lastPosition = firstUpPosition;
					movedWrong = true;
				}
			} else {
				ticksInAir = 0;
				accumulatedUpDistance = 0;
				firstUpPosition = this->position;
			}
		}

		this->previousVelocity = delta;

		// Reset on ground to what the client last claimed
		onGround = savedOnGround;

		// Deal fall damage
		if (Gamerule::Get<bool>(Gamerule::Id::FallDamage)) {
			if (inWater)
				fallDistance = 0;
			if (onGround) {
				if (fallDistance > FALL_DAMAGE_FLOOR) {
					AttackEntityFrom(nullptr, int(std::ceil(fallDistance - FALL_DAMAGE_FLOOR)), DamageType::Fall);
				}
				fallDistance = 0;
			} else if (delta.y < 0) {
				fallDistance -= delta.y;
			}
		}

		auto resolvedDelta = delta;

		// How far is our simulated move vs what the client says?
		// Vanilla ignores Y here
		delta = claimed - this->position;
		delta.y = 0.0;
		double residual = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;

		if (residual < 0.0625) {
			this->position = claimed; // Trust it
			session->position.pos = claimed;
			this->velocity = resolvedDelta;
			// Reset ySize so step up works right
			ySize = 0.0f;
			RebuildCollider();
		} else {
			// Send a correction
			residualTooLarge = true;
		}

		AABB clearCheckArea = collider.Expand(-CLEAR_CHECK_TOLERANCE, -CLEAR_CHECK_TOLERANCE, -CLEAR_CHECK_TOLERANCE);
		auto collidingBoxes = world->GetCollidingBoundingBoxes(clearCheckArea, /*_mover=*/nullptr);
		bool clearNow = collidingBoxes.empty();

		bool willCorrect = (wasClearBefore && (residualTooLarge || !clearNow)) || movedWrong;

		if (movedWrong)
			Logger::Warning << "Client " << session->username << " moved wrongly!\n";

		if (willCorrect) {
			Vec3 safeRollback = lastPosition;
			safeRollback.y += ROLLBACK_NUDGE;
			ticksSinceTeleportSent = 0;

			// TP our player back
			this->Teleport(safeRollback, { rotationYaw, rotationPitch });
			session->position.pos = safeRollback;
			// Wait until our client catches up
			session->pendingTeleport = safeRollback;
			Packet::PlayerPositionAndRotation pkt;
			pkt.rotation = session->rotation;
			pkt.onGround = onGround;
			pkt.position = { safeRollback.x, safeRollback.y + PLAYER_EYE_HEIGHT, safeRollback.z };
			pkt.cameraY = safeRollback.y; // This is backwards, thanks notch
			pkt.Serialize(session->stream);
		}

		// We moved successfully!
		// Walking code
		if (!willCorrect && canTriggerWalking && !(onGround && flags.isSneaking) && vehicle.expired()) {
			Vec3 walkedDelta = this->position - lastPosition; // actual final displacement this tick
			distanceWalkedModified += std::sqrt(walkedDelta.x * walkedDelta.x + walkedDelta.z * walkedDelta.z) * 0.6;

			int bx = MathHelper::FloorDouble(position.x);
			int by = MathHelper::FloorDouble(position.y - 0.2 - yOffset);
			int bz = MathHelper::FloorDouble(position.z);
			BlockType block = world->GetBlockId({ bx, by, bz });
			if (world->GetBlockId({ bx, by - 1, bz }) == BLOCK_FENCE)
				block = world->GetBlockId({ bx, by - 1, bz });

			if (distanceWalkedModified > float(nextStepDistance) && block != BLOCK_AIR) {
				++nextStepDistance;
				if (auto func = Blocks::blockBehaviors[block].onEntityWalking)
					func(*this->world, { bx, by, bz }, *this);
			}
		}

		session->pendingPosition.reset();
	}
}

void EntityMPPlayer::UpdateFallState(float /*_movedY*/) {
	return; // no-op
}

void EntityMPPlayer::DropInventory() {
	for (size_t i = 1; i < session->inventory.slots.size(); i++) {
		auto stack = session->inventory.GetStackInSlot(i);
		if (stack != nullptr) {
			// Create the item entity
			Vec3 itemPos = { position.x, position.y - 0.3 + PLAYER_EYE_HEIGHT, position.z };
			std::shared_ptr<ItemEntity> itemEntity = std::make_shared<ItemEntity>(itemPos);

			ItemStack newStack{ .id = stack->id, .count = stack->count, .data = stack->data };
			itemEntity->itemStack = std::move(newStack);
			itemEntity->pickupCooldown = 40; // So we don't pick it up instantly

			// Give ourselves some random velocity
			float initVelocity = 0.3f;
			float angle = this->rand.NextFloat() * float(JavaMath::PI) * 2.0f;
			itemEntity->velocity.x = double(-std::sin(angle) * initVelocity);
			itemEntity->velocity.z = double(std::cos(angle) * initVelocity);
			itemEntity->velocity.y = 0.2;

			// Register our item with the world
			this->world->entityManager.AddEntity(std::move(itemEntity));

			// Erase this item from the inventory
			stack->count = 0;
			stack->data = 0;
			stack->id = Items::Id::INVALID;
		}
	}
}

void EntityMPPlayer::OnDeath(Entity* _killer, DamageType _type) {
	if (this->isSleeping)
		this->WakeUp();

	PlayerEntity::OnDeath(_killer, _type);

	// Hehe
	if (session && (session->username == "wAidanJC" || session->username == "PixelBrushArt")) {
		session->username == "PixelBrushArt"
		    ? DropItemAtEntity(BLOCK_WOOL, /*Quantity=*/1, /*Data=*/1, /*PickupTime=*/40)
		    : DropItemAtEntity(BLOCK_WOOL, /*Quantity=*/1, /*Data=*/14, /*PickupTime=*/40);
	}

	if (session && session->entityTracker)
		session->entityTracker->RemovePlayer(this);
}

void EntityMPPlayer::Move(Vec3& _velocity) {
	// Remove the step logic so we can do it ourselves in the anti cheat
	Entity::Move(_velocity);
}

void EntityMPPlayer::OnEnterPortal() {
	PlayerEntity::OnEnterPortal();
	
	// Force close gui
	if (this->session) {
		if (session->activeInteraction) 
			session->activeInteraction->OnInteractionClosed(*session->entity);
		PacketUtilities::CloseContainer(*session);
	}
}

ItemStack* EntityMPPlayer::GetHeldItem() {
	return session->inventory.GetHeldItem();
}

void EntityMPPlayer::Tick() {
	if (!session)
		return;

	if (session->connState == ConnectionState::WaitingForSpawnChunks)
		return;

	if (!movedThisTick) {
		// We have to save our ground state here because move will reset it!
		Vec3 none = { 0.0, 0.0, 0.0 };
		bool savedOnGround = onGround;
		Move(none);
		onGround = savedOnGround;
	}


	// Always trust rotations
	this->rotationYaw = session->rotation.x;
	this->rotationPitch = session->rotation.y;

	// Set our held item and armor
	// Slots 5 -> 8 are for armor
	ItemStack none = {};
	auto heldItemPtr = session->inventory.GetHeldItem();
	this->heldItem = session->inventory.GetHeldItem() ? *heldItemPtr : none;
	for (int i = 0; i < 4; i++) {
		auto armorSlotPtr = session->inventory.GetStackInSlot(5 + i);
		this->armor[i] = armorSlotPtr ? armorSlotPtr : nullptr;
	}

	// Do living entity stuff
	PlayerEntity::Tick();

	if (onLadder())
		// No fall damage on ladders
		fallDistance = 0.0f;

	// If we fell out of the world then die
	if (position.y < -64.0)
		OnDeath(nullptr, DamageType::Void);

	if (this->lastNotifiedHealth != GetHeartsHealth()) {
		Packet::SetHealth healthPkt;
		healthPkt.health = GetHeartsHealth();
		healthPkt.Serialize(session->stream);
		this->lastNotifiedHealth = GetHeartsHealth();
	}

	// Tell entities we collided with a player
	if (entityManager) {
		auto entitiesCollidingWith = entityManager->GetEntitiesWithinAabbExcluding(collider.Expand(1.0, 0.0, 1.0),
		                                                                           this->id);
		for (const auto& entity : entitiesCollidingWith) {
			if (!entity->isDead)
				entity->OnCollideWithPlayer(*this);
		}
	}

	// Reset movement flag
	movedThisTick = false;
}