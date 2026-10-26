/*
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../command.h"
#include "../command_manager.h"
#include "../command_registry.h"
#include "entities/entity_boat.h"
#include "entities/entity_chicken.h"
#include "entities/entity_cow.h"
#include "entities/entity_creeper.h"
#include "entities/entity_minecart.h"
#include "entities/entity_painting.h"
#include "entities/entity_pig.h"
#include "entities/entity_sheep.h"
#include "entities/entity_skeleton.h"
#include "entities/entity_slime.h"
#include "entities/entity_spider.h"
#include "entities/entity_zombie.h"
#include "entities/entity_wolf.h"
#include "entities/entity_squid.h"
#include "networking/packets.h"
#include <memory>
#include <utility>

namespace {

EntityType GetEntityTypeFromString(std::string _name) {
	if (_name == "creeper") {
		return EntityType::CREEPER;
	}
	if (_name == "zombie") {
		return EntityType::ZOMBIE;
	}
	if (_name == "skeleton") {
		return EntityType::SKELETON;
	}
	if (_name == "spider") {
		return EntityType::SPIDER;
	}
	if (_name == "sheep") {
		return EntityType::SHEEP;
	}
	if (_name == "cow") {
		return EntityType::COW;
	}
	if (_name == "pig") {
		return EntityType::PIG;
	}
	if (_name == "chicken") {
		return EntityType::CHICKEN;
	}
	if (_name == "squid") {
		return EntityType::SQUID;
	}
	if (_name == "boat") {
		return EntityType::BOAT;
	}
	if (_name == "minecart") {
		return EntityType::MINECART;
	}
	if (_name == "painting") {
		return EntityType::PAINTING;
	}
	if (_name == "slime") {
		return EntityType::SLIME;
	}
	if (_name == "wolf") {
		return EntityType::WOLF;
	}
	return EntityType::NONE;
}

std::shared_ptr<Entity> GetEntityShared(EntityType _type) {
	switch (_type) {
	case EntityType::CREEPER:
		return std::make_shared<CreeperEntity>();
	case EntityType::ZOMBIE:
		return std::make_shared<ZombieEntity>();
	case EntityType::SKELETON:
		return std::make_shared<SkeletonEntity>();
	case EntityType::SPIDER:
		return std::make_shared<SpiderEntity>();
	case EntityType::SHEEP:
		return std::make_shared<SheepEntity>();
	case EntityType::COW:
		return std::make_shared<CowEntity>();
	case EntityType::PIG:
		return std::make_shared<PigEntity>();
	case EntityType::CHICKEN:
		return std::make_shared<ChickenEntity>();
	case EntityType::SQUID:
		return std::make_shared<SquidEntity>();
	case EntityType::BOAT:
		return std::make_shared<BoatEntity>();
	case EntityType::MINECART:
		return std::make_shared<MinecartEntity>();
	case EntityType::SLIME:
		return std::make_shared<SlimeEntity>();
	case EntityType::WOLF:
		return std::make_shared<WolfEntity>();
	default:
		return nullptr;
	}
}

std::string SummonEntity(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto entityType = _cmd.get_arg<std::string>("entityType");
	if (!entityType)
		return ERROR_REASON_PARAMETERS;
	if (!ctx.executionEntity)
		return ERROR_PLAYER_REQUIRED;

	Vec3 spawnPos = ctx.executionEntity->position;
	spawnPos.y += 1.0; // Spawn above the executor, who is one of my favorite pokemon.

	auto eTypeEnum = GetEntityTypeFromString(*entityType);
	if (eTypeEnum == EntityType::NONE)
		return "Unknown entity: " + *entityType;
	auto entity = GetEntityShared(eTypeEnum);
	if (!entity)
		return "Failed to create entity: " + *entityType;

	entity->Teleport(spawnPos);

	ctx.world->entityManager.AddEntity(entity);

	SendFeedback(ctx, std::format("§eSpawned entity at {}!", spawnPos.Str()));
	return "";
}

std::string SummonEntityAtPos(const strategos::CmdNode& _cmd, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto entityType = _cmd.get_arg<std::string>("entityType");
	auto pos = _cmd.get_arg<strategos::Vec3>("pos");
	if (!entityType || !pos)
		return ERROR_REASON_PARAMETERS;

	auto position = ResolveCmdPos(ctx, *pos);
	if (!position) return position.error();
	Vec3 ePos = *position;
	auto eTypeEnum = GetEntityTypeFromString(*entityType);
	if (eTypeEnum == EntityType::NONE)
		return "Unknown entity: " + *entityType;
	auto entity = GetEntityShared(eTypeEnum);
	if (!entity)
		return "Failed to create entity: " + *entityType;

	entity->Teleport(ePos);

	ctx.world->entityManager.AddEntity(entity);

	SendFeedback(ctx, std::format("§eSpawned entity at {}!", ePos.Str()));
	return "";
}

} // namespace

void RegisterSummon(strategos::BrigadierContext& _dispatcher) {
	_dispatcher.add_command(strategos::Node::literal("summon")
	                            .describe("Summons a smart entity")
	                            .op()
	                            .then(strategos::Node::string("entityType")
	                                      .executes(SummonEntity)
	                                      .then(strategos::Node::vec3("pos").executes(SummonEntityAtPos))));
}