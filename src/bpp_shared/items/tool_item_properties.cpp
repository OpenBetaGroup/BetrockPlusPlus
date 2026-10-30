/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#include "../helpers/direction_fixer.h"
#include "blocks.h"
#include "blocks/block_behaviors/internal.h"
#include "entities/entity_painting.h"
#include "item_map.h"
#include "item_properties.h"
#include "items.h"
#include "server.h"
#include "tool_properties.h"

namespace Items {

void RegisterAll() {
	itemBehavior[Items::Id::HOE_WOOD] = ItemBehavior{ .onBlockUse = UseHoe };
	itemBehavior[Items::Id::HOE_STONE] = ItemBehavior{ .onBlockUse = UseHoe };
	itemBehavior[Items::Id::HOE_IRON] = ItemBehavior{ .onBlockUse = UseHoe };
	itemBehavior[Items::Id::HOE_GOLD] = ItemBehavior{ .onBlockUse = UseHoe };
	itemBehavior[Items::Id::HOE_DIAMOND] = ItemBehavior{ .onBlockUse = UseHoe };
	itemBehavior[Items::Id::FLINT_AND_STEEL] = ItemBehavior{ .onBlockUse = UseFlintAndSteel };
	itemBehavior[Items::Id::BUCKET_WATER] = ItemBehavior{ .onBlockUse = UseWaterBucket };
	itemBehavior[Items::Id::BUCKET_LAVA] = ItemBehavior{ .onBlockUse = UseLavaBucket };
	itemBehavior[Items::Id::BUCKET] = ItemBehavior{ .onBlockUse = UseBucket };
	itemBehavior[Items::Id::BOAT] = ItemBehavior{ .onBlockUse = UseBoat };
	itemBehavior[Items::Id::MINECART] = ItemBehavior{ .onBlockUse = UseMinecart };
	itemBehavior[Items::Id::MINECART_CHEST] = ItemBehavior{ .onBlockUse = UseMinecart };
	itemBehavior[Items::Id::MINECART_FURNACE] = ItemBehavior{ .onBlockUse = UseMinecart };
	itemBehavior[Items::Id::BOW] = ItemBehavior{ .onUse = UseBow };
	itemBehavior[Items::Id::EGG] = ItemBehavior{ .onUse = UseThrowable };
	itemBehavior[Items::Id::SNOWBALL] = ItemBehavior{ .onUse = UseThrowable };

	// Tool Properties
	// Sword
	toolProperties[Items::Id::SWORD_WOOD] = ToolProperties{ .type = ToolType::Sword,
		                                                    .material = ToolMaterial::Wooden,
		                                                    .canHarvest = CanShearsOrSwordHarvest,
		                                                    .howEffectiveAgainstBlock = SwordEffectiveness };
	toolProperties[Items::Id::SWORD_STONE] = ToolProperties{ .type = ToolType::Sword,
		                                                     .material = ToolMaterial::Stone,
		                                                     .canHarvest = CanShearsOrSwordHarvest,
		                                                     .howEffectiveAgainstBlock = SwordEffectiveness };
	toolProperties[Items::Id::SWORD_IRON] = ToolProperties{ .type = ToolType::Sword,
		                                                    .material = ToolMaterial::Iron,
		                                                    .canHarvest = CanShearsOrSwordHarvest,
		                                                    .howEffectiveAgainstBlock = SwordEffectiveness };
	toolProperties[Items::Id::SWORD_GOLD] = ToolProperties{ .type = ToolType::Sword,
		                                                    .material = ToolMaterial::Gold,
		                                                    .canHarvest = CanShearsOrSwordHarvest,
		                                                    .howEffectiveAgainstBlock = SwordEffectiveness };
	toolProperties[Items::Id::SWORD_DIAMOND] = ToolProperties{ .type = ToolType::Sword,
		                                                       .material = ToolMaterial::Diamond,
		                                                       .canHarvest = CanShearsOrSwordHarvest,
		                                                       .howEffectiveAgainstBlock = SwordEffectiveness };
	// Pickaxe
	toolProperties[Items::Id::PICKAXE_WOOD] = ToolProperties{ .type = ToolType::Pickaxe,
		                                                      .material = ToolMaterial::Wooden,
		                                                      .canHarvest = CanPickaxeHarvest,
		                                                      .howEffectiveAgainstBlock = ToolEffectiveness };
	toolProperties[Items::Id::PICKAXE_STONE] = ToolProperties{ .type = ToolType::Pickaxe,
		                                                       .material = ToolMaterial::Stone,
		                                                       .canHarvest = CanPickaxeHarvest,
		                                                       .howEffectiveAgainstBlock = ToolEffectiveness };
	toolProperties[Items::Id::PICKAXE_IRON] = ToolProperties{ .type = ToolType::Pickaxe,
		                                                      .material = ToolMaterial::Iron,
		                                                      .canHarvest = CanPickaxeHarvest,
		                                                      .howEffectiveAgainstBlock = ToolEffectiveness };
	toolProperties[Items::Id::PICKAXE_GOLD] = ToolProperties{ .type = ToolType::Pickaxe,
		                                                      .material = ToolMaterial::Gold,
		                                                      .canHarvest = CanPickaxeHarvest,
		                                                      .howEffectiveAgainstBlock = ToolEffectiveness };
	toolProperties[Items::Id::PICKAXE_DIAMOND] = ToolProperties{ .type = ToolType::Pickaxe,
		                                                         .material = ToolMaterial::Diamond,
		                                                         .canHarvest = CanPickaxeHarvest,
		                                                         .howEffectiveAgainstBlock = ToolEffectiveness };
	// Axe
	toolProperties[Items::Id::AXE_WOOD] = ToolProperties{ .type = ToolType::Axe,
		                                                  .material = ToolMaterial::Wooden,
		                                                  .howEffectiveAgainstBlock = ToolEffectiveness };
	toolProperties[Items::Id::AXE_STONE] = ToolProperties{ .type = ToolType::Axe,
		                                                   .material = ToolMaterial::Stone,
		                                                   .howEffectiveAgainstBlock = ToolEffectiveness };
	toolProperties[Items::Id::AXE_IRON] = ToolProperties{ .type = ToolType::Axe,
		                                                  .material = ToolMaterial::Iron,
		                                                  .howEffectiveAgainstBlock = ToolEffectiveness };
	toolProperties[Items::Id::AXE_GOLD] = ToolProperties{ .type = ToolType::Axe,
		                                                  .material = ToolMaterial::Gold,
		                                                  .howEffectiveAgainstBlock = ToolEffectiveness };
	toolProperties[Items::Id::AXE_DIAMOND] = ToolProperties{ .type = ToolType::Axe,
		                                                     .material = ToolMaterial::Diamond,
		                                                     .howEffectiveAgainstBlock = ToolEffectiveness };
	// Shovel
	toolProperties[Items::Id::SHOVEL_WOOD] = ToolProperties{ .type = ToolType::Shovel,
		                                                     .material = ToolMaterial::Wooden,
		                                                     .canHarvest = CanShovelHarvest,
		                                                     .howEffectiveAgainstBlock = ToolEffectiveness };
	toolProperties[Items::Id::SHOVEL_STONE] = ToolProperties{ .type = ToolType::Shovel,
		                                                      .material = ToolMaterial::Stone,
		                                                      .canHarvest = CanShovelHarvest,
		                                                      .howEffectiveAgainstBlock = ToolEffectiveness };
	toolProperties[Items::Id::SHOVEL_IRON] = ToolProperties{ .type = ToolType::Shovel,
		                                                     .material = ToolMaterial::Iron,
		                                                     .canHarvest = CanShovelHarvest,
		                                                     .howEffectiveAgainstBlock = ToolEffectiveness };
	toolProperties[Items::Id::SHOVEL_GOLD] = ToolProperties{ .type = ToolType::Shovel,
		                                                     .material = ToolMaterial::Gold,
		                                                     .canHarvest = CanShovelHarvest,
		                                                     .howEffectiveAgainstBlock = ToolEffectiveness };
	toolProperties[Items::Id::SHOVEL_DIAMOND] = ToolProperties{ .type = ToolType::Shovel,
		                                                        .material = ToolMaterial::Diamond,
		                                                        .canHarvest = CanShovelHarvest,
		                                                        .howEffectiveAgainstBlock = ToolEffectiveness };

	// Hoe
	toolProperties[Items::Id::HOE_WOOD] = ToolProperties{
		.type = ToolType::Hoe,
		.material = ToolMaterial::Wooden,
	};
	toolProperties[Items::Id::HOE_STONE] = ToolProperties{
		.type = ToolType::Hoe,
		.material = ToolMaterial::Stone,
	};
	toolProperties[Items::Id::HOE_IRON] = ToolProperties{
		.type = ToolType::Hoe,
		.material = ToolMaterial::Iron,
	};
	toolProperties[Items::Id::HOE_GOLD] = ToolProperties{
		.type = ToolType::Hoe,
		.material = ToolMaterial::Gold,
	};
	toolProperties[Items::Id::HOE_DIAMOND] = ToolProperties{
		.type = ToolType::Hoe,
		.material = ToolMaterial::Diamond,
	};
	// Apply max uses based on material
	for (auto& toolProperty : toolProperties) {
		toolProperty.second.maxUses = GetMaterialUses(toolProperty.second.material);
	}
	// Misc tools
	toolProperties[Items::Id::FLINT_AND_STEEL] = ToolProperties{ .type = ToolType::None,
		                                                         .material = ToolMaterial::None,
		                                                         .maxUses = DURABILITY_FLINT_AND_STEEL };
	toolProperties[Items::Id::SHEARS] = ToolProperties{ .type = ToolType::None,
		                                                .material = ToolMaterial::None,
		                                                .maxUses = DURABILITY_SHEARS,
		                                                .canHarvest = CanShearsOrSwordHarvest,
		                                                .howEffectiveAgainstBlock = ShearsEffectiveness };
	toolProperties[Items::Id::BOW] = ToolProperties{ .type = ToolType::None,
		                                             .material = ToolMaterial::None,
		                                             .maxUses = DURABILITY_BOW };
	toolProperties[Items::Id::FISHING_ROD] = ToolProperties{ .type = ToolType::None,
		                                                     .material = ToolMaterial::None,
		                                                     .maxUses = DURABILITY_FISHING_ROD };

	// Armor Leather
	toolProperties[Items::Id::HELMET_LEATHER] = ToolProperties{ .maxUses = DURABILITY_HELMET_LEATHER };
	toolProperties[Items::Id::CHESTPLATE_LEATHER] = ToolProperties{ .maxUses = DURABILITY_CHEST_LEATHER };
	toolProperties[Items::Id::LEGGINGS_LEATHER] = ToolProperties{ .maxUses = DURABILITY_LEGS_LEATHER };
	toolProperties[Items::Id::BOOTS_LEATHER] = ToolProperties{ .maxUses = DURABILITY_BOOTS_LEATHER };

	// Armor Chainmail
	toolProperties[Items::Id::HELMET_CHAINMAIL] = ToolProperties{ .maxUses = DURABILITY_HELMET_CHAINMAIL };
	toolProperties[Items::Id::CHESTPLATE_CHAINMAIL] = ToolProperties{ .maxUses = DURABILITY_CHEST_CHAINMAIL };
	toolProperties[Items::Id::LEGGINGS_CHAINMAIL] = ToolProperties{ .maxUses = DURABILITY_LEGS_CHAINMAIL };
	toolProperties[Items::Id::BOOTS_CHAINMAIL] = ToolProperties{ .maxUses = DURABILITY_BOOTS_CHAINMAIL };

	// Armor Iron
	toolProperties[Items::Id::HELMET_IRON] = ToolProperties{ .maxUses = DURABILITY_HELMET_IRON };
	toolProperties[Items::Id::CHESTPLATE_IRON] = ToolProperties{ .maxUses = DURABILITY_CHEST_IRON };
	toolProperties[Items::Id::LEGGINGS_IRON] = ToolProperties{ .maxUses = DURABILITY_LEGS_IRON };
	toolProperties[Items::Id::BOOTS_IRON] = ToolProperties{ .maxUses = DURABILITY_BOOTS_IRON };

	// Armor Diamond
	toolProperties[Items::Id::HELMET_DIAMOND] = ToolProperties{ .maxUses = DURABILITY_HELMET_DIAMOND };
	toolProperties[Items::Id::CHESTPLATE_DIAMOND] = ToolProperties{ .maxUses = DURABILITY_CHEST_DIAMOND };
	toolProperties[Items::Id::LEGGINGS_DIAMOND] = ToolProperties{ .maxUses = DURABILITY_LEGS_DIAMOND };
	toolProperties[Items::Id::BOOTS_DIAMOND] = ToolProperties{ .maxUses = DURABILITY_BOOTS_DIAMOND };

	// Armor Gold
	toolProperties[Items::Id::HELMET_GOLD] = ToolProperties{ .maxUses = DURABILITY_HELMET_GOLD };
	toolProperties[Items::Id::CHESTPLATE_GOLD] = ToolProperties{ .maxUses = DURABILITY_CHEST_GOLD };
	toolProperties[Items::Id::LEGGINGS_GOLD] = ToolProperties{ .maxUses = DURABILITY_LEGS_GOLD };
	toolProperties[Items::Id::BOOTS_GOLD] = ToolProperties{ .maxUses = DURABILITY_BOOTS_GOLD };

	// Tool behaviors
	for (const auto& [itemId, props] : toolProperties) {
		ToolBehavior behavior{};
		switch (props.type) {
		case ToolType::Sword:
			behavior.onBlockFinishMining = OnToolFinishMining;
			behavior.onEntityAttack = AttackWithItem;
			break;
		case ToolType::Pickaxe:
		case ToolType::Axe:
		case ToolType::Shovel:
			behavior.onBlockFinishMining = OnToolFinishMining;
			behavior.onEntityAttack = AttackWithItem;
			break;
		case ToolType::Hoe:
			itemBehavior[itemId] = ItemBehavior{ .onBlockUse = UseHoe };
			break;
		case ToolType::None:
			if (itemId == Items::Id::SHEARS)
				behavior.onBlockFinishMining = OnToolFinishMining;
			break;
		}
		toolBehavior[itemId] = behavior;
	}

	// Item used on entity behaviors
	toolBehavior[SHEARS].onEntityUse = UseShears;
	toolBehavior[BUCKET].onEntityUse = UseBucketOnEntity;
	toolBehavior[SADDLE].onEntityUse = UseSaddle;
	toolBehavior[DYE].onEntityUse = UseDye;

	// Item behaviors
	itemBehavior[APPLE].onUse = EatFood;
	itemBehavior[BREAD].onUse = EatFood;
	itemBehavior[PORKCHOP].onUse = EatFood;
	itemBehavior[PORKCHOP_COOKED].onUse = EatFood;
	itemBehavior[APPLE_GOLDEN].onUse = EatFood;
	itemBehavior[COOKIE].onUse = EatFood;
	itemBehavior[FISH].onUse = EatFood;
	itemBehavior[FISHING_ROD].onUse = UseFishingRod;
	itemBehavior[FISH_COOKED].onUse = EatFood;
	itemBehavior[MUSHROOM_STEW].onUse = EatFood;

	itemBehavior[SUGARCANE].onBlockUse = [](WorldManager& _world, ItemStack* _stack, Int3 _pos, Entity& /*_user*/,
	                                        Cardinal::Value _face) {
		Int3 placePos = _pos.WithOffset(_face);
		if (!Blocks::CanSugarcaneSurviveAt(_world, placePos))
			return;

		_world.SetBlock(placePos, BLOCK_SUGARCANE);
		_stack->DecrementCount(1);
	};

	// Happy 23rd Birthday to me! (Pixel Brush)
	itemBehavior[CAKE].onBlockUse = [](WorldManager& _world, ItemStack* _stack, Int3 _pos, Entity& /*_user*/,
	                                   Cardinal::Value _face) {
		Int3 placePos = _pos.WithOffset(_face);
		if (!Blocks::CanCakeSurviveAt(_world, placePos))
			return;

		_world.SetBlock(placePos, BLOCK_CAKE);
		_stack->DecrementCount(1);
	};

	itemBehavior[SEEDS_WHEAT].onBlockUse = [](WorldManager& _world, ItemStack* _stack, Int3 _pos, Entity& /*_user*/,
	                                          Cardinal::Value _face) {
		Int3 placePos = _pos.WithOffset(_face);
		if (!Blocks::CanCropsSurviveAt(_world, placePos))
			return;

		_world.SetBlock(placePos, BLOCK_CROP_WHEAT);
		_stack->DecrementCount(1);
	};

	itemBehavior[PAINTING].onBlockUse = [](WorldManager& _world, ItemStack* _stack, Int3 _pos, Entity& /*_user*/,
	                                       Cardinal::Value _face) {
		if (_face == Cardinal::Value::Down || _face == Cardinal::Value::Up)
			return;

		PaintingEntity painting(_pos, _face);
		painting.world = &_world;
		painting.entityManager = &_world.entityManager;
		if (painting.PickValidArt()) {
			_world.entityManager.AddEntity(std::make_shared<PaintingEntity>(painting));
			_stack->DecrementCount(1);
		}
	};

	itemBehavior[SIGN].onBlockUse = [](WorldManager& _world, ItemStack* _stack, Int3 _pos, Entity& _user,
	                                   Cardinal::Value _face) {
		Int3 placePos = _pos.WithOffset(_face);
		bool success = false;
		if (_face == Cardinal::Value::Up) {
			int meta = MathHelper::FloorDouble(((_user.rotationYaw + 180.0f) / 360.0f) * 16.0f + 0.5f) & 0xF;
			success = Blocks::blockBehaviors[BLOCK_SIGN_STANDING].onBlockPlaced(_world, placePos, _user, _face,
			                                                                    BLOCK_SIGN_STANDING, meta);
		} else {
			success = Blocks::blockBehaviors[BLOCK_SIGN_WALL].onBlockPlaced(
			    _world, placePos, _user, _face, BLOCK_SIGN_WALL, GetMetaFromDirection(BLOCK_SIGN_WALL, _face));
		}
		if (success) 
			_stack->DecrementCount(1);
	};

	auto onDoorPlace = [](WorldManager& _world, ItemStack* _stack, Int3 _pos, Entity& _user, Cardinal::Value _face) {
		BlockType targetBlockType;
		targetBlockType = _stack->id == DOOR_WOOD ? BLOCK_DOOR_WOOD : BLOCK_DOOR_IRON;
		Int3 placePosition = _pos.WithOffset(_face);
		if (Blocks::blockBehaviors[targetBlockType].onBlockPlaced(_world, placePosition, _user, _face, targetBlockType,
		                                                          0))
			_stack->DecrementCount(1);
	};

	itemBehavior[DOOR_WOOD].onBlockUse = onDoorPlace;
	itemBehavior[DOOR_IRON].onBlockUse = onDoorPlace;

	itemBehavior[REDSTONE_REPEATER].onBlockUse = [](WorldManager& _world, ItemStack* _stack, Int3 _pos, Entity& _user,
	                                                Cardinal::Value _face) {
		Int3 placePos = _pos.WithOffset(_face);
		if (Blocks::blockBehaviors[BLOCK_REDSTONE_REPEATER_OFF].onBlockPlaced(_world, placePos, _user, _face,
		                                                                      BLOCK_REDSTONE_REPEATER_OFF, 0))
			_stack->DecrementCount(1);
	};

	itemBehavior[REDSTONE].onBlockUse = [](WorldManager& _world, ItemStack* _stack, Int3 _pos, Entity& _user,
	                                       Cardinal::Value _face) {
		Int3 placePos = _pos.WithOffset(_face);
		if (Blocks::blockBehaviors[BLOCK_REDSTONE].onBlockPlaced(_world, placePos, _user, _face, BLOCK_REDSTONE, 0))
			_stack->DecrementCount(1);
	};

	itemBehavior[BED].onBlockUse = [](WorldManager& _world, ItemStack* _stack, Int3 _pos, Entity& _user,
	                                  Cardinal::Value _face) {
		Int3 placePos = _pos.WithOffset(_face);
		if (Blocks::blockBehaviors[BLOCK_BED].onBlockPlaced(_world, placePos, _user, _face, BLOCK_BED, 0))
			_stack->DecrementCount(1);
	};

	itemBehavior[DYE].onBlockUse = [](WorldManager& _world, ItemStack* _stack, Int3 _pos, Entity& /*_user*/,
	                                  Cardinal::Value /*_face*/) {
		// Bonemeal has metadata 15
		if (_stack->data != 15)
			return;

		auto thisBlock = _world.GetBlockId(_pos);
		// Grow tree
		if (thisBlock == BLOCK_SAPLING) {
			_stack->DecrementCount(1);
			Blocks::TryGrowTree(_world, _pos);
			return;
		}

		// Grow crops
		if (thisBlock == BLOCK_CROP_WHEAT) {
			_stack->DecrementCount(1);
			_world.SetMeta(_pos, 7);
			return;
		}

		// This is fun!
		// Try and spread decorations on grass
		if (thisBlock == BLOCK_GRASS) {
			_stack->DecrementCount(1);
			auto& random = _world.rand;

			for (size_t i = 0; i < 128; i++) {
				Int3 pos = _pos.WithOffset(Cardinal::Value::Up);
				bool validWalk = true;

				for (size_t step = 0; step < (i / 16); step++) {
					pos.x += random.NextInt(3) - 1;
					pos.y += (random.NextInt(3) - 1) * random.NextInt(3) / 2;
					pos.z += random.NextInt(3) - 1;
					if (_world.GetBlockId(pos.WithOffset(Cardinal::Value::Down)) != BLOCK_GRASS ||
					    _world.IsBlockNormalCube(pos)) {
						validWalk = false;
						break; // abort the attempt
					}
				}
				if (!validWalk)
					continue; // move on to attempt i+1

				if (_world.GetBlockId(pos) == BLOCK_AIR) {
					if (random.NextInt(10) != 0) {
						_world.SetBlock(pos, BLOCK_TALLGRASS, /*meta=*/1);
					} else if (random.NextInt(3) != 0) {
						_world.SetBlock(pos, BLOCK_DANDELION);
					} else {
						_world.SetBlock(pos, BLOCK_ROSE);
					}
				}
			}
			return;
		}
	};

	// TODO: Implement maps properly
	itemBehavior[MAP].onStartHolding = [](ItemStack* /*_stack*/, PlayerSession& /*_session*/) {
		Logger::Debug << "Started holding a map!\n";
	};

	itemBehavior[MAP].whileHeld = [](ItemStack* /*_stack*/, PlayerSession& _session, Server& _server) {
		auto pos = _session.position.GetBlockPos();
		std::vector<Map::MarkerPlacement> markers = {
			// TODO: Evil fuck-ass offset magic???
			Map::MakeMarker(Map::Icon::WhiteArrow, _session.rotation.x,
			                Byte2{ static_cast<int8_t>(pos.x), static_cast<int8_t>(pos.z) })
		};
		std::vector<uint8_t> mapIcons;
		PlaceMarkers(mapIcons, markers);
		Packet::ItemData pkt;
		pkt.itemId = MAP;
		pkt.mapId = 0;
		pkt.data = mapIcons;
		pkt.Serialize(_session.stream);

		static int8_t x = 0;
		if (x >= INT8_MAX - 1)
			return;

		// TODO: Maps are 1:8 in B1.7.3
		// TODO: Maps were centered on the point they were crafted in B1.7.3
		auto world = _server.GetWorldForDimension(_session.dimension);
		std::vector<uint8_t> mapData;
		Map::InitGraphics(mapData, Byte2{ x, 0 });

		for (int z = 0; z < INT8_MAX; z++) {
			int8_t northY = world->GetHeightValue(x, z - 1);
			int8_t y = world->GetHeightValue(x, z);
			Int3 bpos{ x, y - 1, z };
			BlockType block = world->GetBlockId(bpos);
			int8_t brightness = 1;
			if (northY > y)
				brightness--;
			if (northY < y)
				brightness++;
			Map::AppendPixel(mapData, (Blocks::blockProperties[block].material.mapColor.index << 2) | brightness);
		}

		Packet::ItemData gfxPkt;
		gfxPkt.itemId = MAP;
		gfxPkt.mapId = 0;
		gfxPkt.data = mapData;
		gfxPkt.Serialize(_session.stream);
		x++;
	};

	itemBehavior[MAP].onStopHolding = [](ItemStack* /*_stack*/) {
		Logger::Debug << "No longer holding a map!\n";
	};
};
}; // namespace Items