/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once
#include "entities.h"
#include "entities/entity_mp_player.h"
#include "inventory/item_stack.h"
#include "runtime.h"
#include "server.h"

struct PlayerSessionMP;
struct DummyMPPlayer : public EntityMPPlayer {
	PlayerSessionMP dummySession;
	Server& server;
	DummyMPPlayer(Runtime& _runtime, Server& _server) : EntityMPPlayer(), dummySession(-1, _runtime), server(_server) {
		hasPhysics = true;
	}
	~DummyMPPlayer() {
		session = nullptr;
	}

	void Tick() override;
};