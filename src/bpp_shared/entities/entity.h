/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#pragma once
#include "base_types.h"
#include "blocks/block_properties.h"
#include "dimensions.h"
#include "entities.h"
#include "helpers/aabb.h"
#include "helpers/java/java_math.h"
#include "helpers/java/java_random.h"
#include "lighter.h"
#include "nbt/nbt.h"
#include "numeric_structs.h"
#include "packet_data.h"
#include "damage_types.h"
#include <optional>
#include <vector>

#ifdef BUILD_SERVER
#include "addon/addon_impl.h"
#endif

struct EntityFlags {
	bool isBurning = false;
	bool isSneaking = false;
	bool isRiding = false;
};

// Constants pulled from the betaWiki!
// https://pixelbrush.dev/beta-wiki/entities/movement
// I <3 BETA WIKI!
const float KNOCKBACK_VELOCITY_DAMPENING = 0.5f;
const float HORIZONTAL_KNOCKBACK = 0.4f;
const float VERTICAL_KNOCKBACK = 0.4f;

const float COBWEB_VERTICAL_DRAG = 0.05f;
const float COBWEB_HORIZONTAL_DRAG = 0.25f;

const float LADDER_MAX_HORIZONTAL = 0.15f;
const float LADDER_MAX_DESCENT = 0.15f;
const float LADDER_SNEAK_DESCENT = 0.0f;
const float LADDER_WALL_BOOST = 0.2f;

const float WATER_DRAG = 0.8f;
const float LAVA_DRAG = 0.5f;
const float FLUID_GRAVITY = 0.02f;
const float FLUID_ACCELERATION = 0.02f;
const float FLUID_WALL_BOOST = 0.3f;

const float GRAVITY = 0.08f;
const float VERTICAL_FRICTION = 0.98f;
const float HORIZONTAL_FRICTION = 0.91f;
const float JUMP_VELOCITY = 0.42f;
const float FLUID_JUMP_BOOST = 0.04f;
const float FALL_DAMAGE_FLOOR = 3.0f;
const float STEP_HEIGHT = 0.5f;
const float DEFAULT_BLOCK_SLIPPERINESS = 0.6f;
const float NORMAL_FRICTION_CUBED = 0.16277136f;
const float AIR_ACCELERATION = 0.02f;
const float INPUT_DECAY = 0.98f;
const float SNEAK_SPEED_MODIFIER = 0.3f;

struct PlayerEntity;
struct EntityManager;
struct LightningEntity;

// TODO: Reorder some of these. Having a bunch of bools
// like this between bigger types is bad for alignment
struct Entity {
#ifdef BUILD_SERVER // Maybe this shouldn't be here
	bp_entity apiEntity;
#endif

	// For randomness
	Java::Random rand;

	// Entity type because notch split stuff into multiple packets based on type
	EntityType type = EntityType::NONE;

	// World pointer
	WorldManager* world = nullptr;
	EntityManager* entityManager = nullptr;

	// For queueing events
	std::vector<PacketData::EntityEvent> pendingEvents;

	// Identity
	EntityId id = -1; // -1 = not yet spawned
	bool isDead = false;
	TickTime ticksExisted = 0;
	Dimension dim = Dimension::Overworld;

	// Mob links
	std::weak_ptr<Entity> vehicle;
	std::weak_ptr<Entity> passenger;

	Vec3 position;
	Vec3 velocity;
	bool forceVelocityUpdate = false;

	// Look direction
	float rotationYaw = 0.0f;
	float rotationPitch = 0.0f;

	// Collision
	AABB collider;
	Int3 bucketPos; // The bucket this entity is currently in (for spatial partitioning)
	Blocks::BlockProperties belowBlock;

	// Width/height of the collision box in blocks.
	float width = 0.6f;
	float height = 1.8f;

	// Vanilla uses this field quite confusingly; we do not do the same
	// position.y is FEET!
	// yOffset nudges the collider DOWN as it INCREASES
	// so a yOffset of 1.62 would nudge the collider down by 1.62
	float yOffset = 0.0f;

	// How high a block face this entity can step onto without jumping.
	float stepHeight = 0.0f;

	// Collision state
	bool onGround : 1 = true;
	bool collided : 1 = false;
	bool collidedHorizontally : 1 = false;
	bool collidedVertically : 1 = false;

	// Does this entity act as a block collider?
	bool actsAsWorldCollider : 1 = false;

	// Movement / environment state
	bool hasPhysics : 1 = true;
	bool inWeb : 1 = false; // Inside a cobweb
	bool inWater : 1 = false;
	bool inLava : 1 = false;
	bool onLadder : 1 = false;

	float fallDistance = 0.0f;

	// Just an interpolated offset so stepping up ledges is smooth
	float ySize = 0.0f;

	// Yaw, pitch smoothing
	Float2 passengerLookDelta = { 0.0f, 0.0f };
	// The vehicle's rotationYaw/rotationPitch as of the last tick
	Float2 lastVehicleRotation = { 0.0f, 0.0f };

	// Inputs
	Float2 input;
	//bool sneaking = false;
	bool jumping = false;

	// Fire
	int fireTicks =
	    0; // > 0: burning ticks left; <= 0: not burning (negative = grace period, see fireResistance)	bool inFire = false;         // Currently touching a fire/lava block
	int fireResistance = 1;      // Ticks of immunity after catching fire
	bool isImmuneToFire = false; // Total fire immunity

	// Combat
	bool beenAttacked = false;
	int hurtResistantTime = 0;  // Invincibility frames after being hit
	float attackedAtYaw = 0.0f; // Yaw from which the last attack came

	// Spawning
	bool preventEntitySpawning = false;
	bool isFirstUpdate = true; // True only on the very first Tick

	// Air
	int maxAir = 300;
	int air = 300;

	// Metadata flags
	EntityFlags flags;
	bool wasMetadataUpdated = false;

	float entityBrightness = 0.0f;

	Entity()
#ifdef BUILD_SERVER
	    : apiEntity(this)
#endif
	{
		RebuildCollider();
	}
	virtual ~Entity() = default;
	virtual void EncodeMetadata(std::vector<PacketData::EntityMetadata::DataEntry>& _metadata);
	virtual bool DecodeMetadata(const std::vector<PacketData::EntityMetadata::DataEntry>& _metadata);
	virtual void Tick();
	virtual void TickPassengerEntity();
	virtual bool PushOutOfBlocks(Vec3 _pos);
	virtual void OnCollideWithPlayer(PlayerEntity& _entity);
	virtual void ApplyKnockback(Vec3 _direction);
	virtual void ApplyInput(float _acceleration);
	virtual void Move(Vec3& _velocity);
	virtual void UpdateFallState(float _movedY);
	virtual std::optional<Tag> SerializeToNbt();
	virtual void LoadFromNbt(Tag& _nbt);
	virtual void DropItemAtEntity(ItemId _itemId, ItemAmount _count, ItemDamage _data = 0, int _pickupTime = 10);
	virtual void OnPlayerInteract(PlayerEntity* _entity);
	virtual void UpdateEntityPhysicsState();
	virtual void OnMountEntity();
	virtual void OnDismountEntity();
	virtual void OnEnterPortal();
	float GetEntityBrightnessValue();
	void MountEntity(std::shared_ptr<Entity> _entity);
	void UnmountEntity();
	bool IsWet();

	virtual bool CanBePushed() {
		return false;
	}

	virtual bool CanBeCollidedWith() {
		return false;
	}

	virtual std::optional<AABB> GetMoverCollisionOverride(Entity& /*_candidate*/) {
		return std::nullopt;
	}

	virtual float GetMountOffset() {
		return this->height * 0.75;
	}

	virtual float GetRidingYOffset() {
		return yOffset;
	}

	virtual Vec3 GetRiderSeatOffset() {
		return { 0.0, 0.0, 0.0 };
	}

	void QueueEvent(PacketData::EntityEvent _e) {
		this->pendingEvents.push_back(_e);
	}

	void RebuildCollider() {
		double halfWidth = double(width) / 2.0;
		double bottom = position.y - double(yOffset) + double(ySize);
		collider = { position.x - halfWidth,  bottom,
			         position.z - halfWidth,  position.x + halfWidth,
			         bottom + double(height), position.z + halfWidth };
	}

	void SetSize(Vec2 _size) {
		this->width = _size.x;
		this->height = _size.y;
		this->RebuildCollider();
	}

	Vec3 GetLookVector(float _yaw, float _pitch) {
		double yawRad = _yaw * (JavaMath::PI / 180.0);
		double pitchRad = _pitch * (JavaMath::PI / 180.0);

		double cosPitch = std::cos(pitchRad);
		return { -std::sin(yawRad) * cosPitch, -std::sin(pitchRad), std::cos(yawRad) * cosPitch };
	}

	Int3 GetPositionAsBlockPosition(Vec3& _position) {
		auto fd = MathHelper::FloorDouble;
		return { fd(_position.x), fd(_position.y), fd(_position.z) };
	}

	virtual void OnStruckByLightning(LightningEntity& _bolt);

	void Teleport(Vec3 _newpos, Vec2 _newrot = { 0, 0 }) {
		position.x = _newpos.x;
		position.y = _newpos.y;
		position.z = _newpos.z;
		rotationYaw = _newrot.x;
		rotationPitch = _newrot.y;
		ySize = 0.0f;
		velocity.x = 0.0;
		velocity.y = 0.0;
		velocity.z = 0.0;
		fallDistance = 0.0;
		onGround = true;
		RebuildCollider();
	}

	virtual bool AttackEntityFrom(Entity* _entity, int _damage, DamageType _type) {
		beenAttacked = true;
		forceVelocityUpdate = true;
		return false;
	}
	virtual AABB GetFluidCollider() {
		// Returns the collider we use to compare if we are in a fluid
		return collider.Expand(0.0, -0.4, 0.0);
	}
	virtual AABB GetLavaCollider() {
		// Returns the collider we use to detect if we are in lava
		return collider.Expand(-0.1, -0.4, -0.1);
	}
	virtual AABB GetFireCollider() {
		// Returns the collider we use to detect if we are in something flammable
		return collider.Expand(-0.001, -0.001, -0.001);
	}
	template <typename T>
	void UpdateMetadata(T& _flag, T _value) {
		if (_flag != _value) {
			_flag = _value;
			wasMetadataUpdated = true;
		}
	}

protected:
	template <typename T>
	inline const T* FindMetadata(const std::vector<PacketData::EntityMetadata::DataEntry>& _metadata,
	                             PacketData::EntityMetadata::Type _desiredType, uint8_t _desiredIndex) {
		for (auto& m : _metadata) {
			if (m.type != _desiredType || m.index != _desiredIndex)
				continue;
			// Ew, gross, a pointer!
			return &std::get<T>(m.value);
		}
		return nullptr;
	}
};