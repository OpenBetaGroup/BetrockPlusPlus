/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#pragma once
#include "entity_mobile.h"
#include "raycast.h"

struct MobEntity : public MobileEntity {
	std::weak_ptr<Entity> target;
	TickTime ticksToFollowTarget = 0;
	float randomYawVelocity = 0.0f;
	double defaultPitch = 0.0;
	bool isHostile = false;

	MobEntity() : MobileEntity() {
		width = 0.9f;
		height = 0.9f;
		stepHeight = 0.5f;
		SetMaxHealth(/*Health=*/10);
	}
	~MobEntity() = default;
	void FaceEntity(MobileEntity& _entity, float _maxYaw, float _maxPitch);
	virtual void UpdateAIState() override;
	virtual void Wander();
	virtual float GetWanderWeight(Int3 _pos);
	virtual void UpdateState();
	virtual bool TryDespawn();
	virtual void OnDeath(Entity* _killer, DamageType _type) override;
	virtual void Tick() override;
	virtual bool CanSpawnAt() override {
		if (!world)
			return false;
		return MobileEntity::CanSpawnAt();
	}
	bool HasLineOfSight(Vec3 _from, Vec3 _to) {
		if (!world)
			return false;
		RayCastResult result = Raycast::Raycast(*this->world, _from, _to, RayCastMode::IGNORE_FLUIDS);
		return !result.hit;
	}

	double GetDesiredRotation(double _rotation, double _desired, double _max) {
		auto delta = _desired - _rotation;
		while (delta < -180.0)
			delta += 360.0;
		while (delta >= 180.0)
			delta -= 360.0;
		return _rotation + std::clamp(delta, -_max, _max);
	}
};