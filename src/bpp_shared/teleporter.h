/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once
#include "entities/entity.h"

// Responsible for handling portal placement validation and creation

class WorldManager;
namespace Teleporter {
void PlaceAtValidTransferSpot(WorldManager& _world, Entity& _entity);
bool PlaceInExistingPortal(WorldManager& _world, Entity& _entity);
bool CreatePortal(WorldManager& _world, Entity& _entity);
} 