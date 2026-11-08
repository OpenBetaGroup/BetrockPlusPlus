/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "blocks.h"
#include "blocks/block_behaviors.h"
#include "blocks/block_properties.h"
#include "dimensions.h"
#include "entities/entity_falling_block.h"
#include "entities/entity_player.h"
#include "entities/entity_skeleton.h"
#include "entities/entity_spider.h"
#include "entities/entity_zombie.h"
#include "enums/items.h"
#include "generator/overworld/tree_gen.h"
#include "helpers/direction_fixer.h"
#include "helpers/java/java_math.h"
#include "internal.h"
#include "items/item_properties.h"
#include "logger.h"
#include "managers/rail_manager.h"
#include "managers/redstone_manager.h"
#include "managers/tick_scheduler.h"
#include "numeric_structs.h"
#include "packet_data.h"
#include "tile_entities/tile_entity.h"
#include "world.h"

namespace Blocks {

void RegisterBlockDrops() {
	// --------------- block drops, only exceptions are included (something that doesn't drop itself) ---------------
	blockBehaviors[BLOCK_STONE].idDropped = [](uint8_t, Java::Random&) -> ItemId {
		return BLOCK_COBBLESTONE;
	};
	blockBehaviors[BLOCK_GRASS].idDropped = [](uint8_t, Java::Random&) -> ItemId {
		return BLOCK_DIRT;
	};
	blockBehaviors[BLOCK_FARMLAND].idDropped = [](uint8_t, Java::Random&) -> ItemId {
		return BLOCK_DIRT;
	};
	blockBehaviors[BLOCK_ORE_COAL].idDropped = [](uint8_t, Java::Random&) -> ItemId {
		return Items::Id::COAL;
	};
	blockBehaviors[BLOCK_ORE_DIAMOND].idDropped = [](uint8_t, Java::Random&) -> ItemId {
		return Items::Id::DIAMOND;
	};
	blockBehaviors[BLOCK_REDSTONE].idDropped = [](uint8_t, Java::Random&) -> ItemId {
		return Items::Id::REDSTONE;
	};
	blockBehaviors[BLOCK_SUGARCANE].idDropped = [](uint8_t, Java::Random&) -> ItemId {
		return Items::Id::SUGARCANE;
	};
	blockBehaviors[BLOCK_COBWEB].idDropped = [](uint8_t, Java::Random&) -> ItemId {
		return Items::Id::STRING;
	};
	blockBehaviors[BLOCK_DEADBUSH].idDropped = [](uint8_t, Java::Random&) -> ItemId {
		return Items::Id::INVALID;
	};
	blockBehaviors[BLOCK_STAIRS_WOOD].idDropped = [](uint8_t, Java::Random&) -> ItemId {
		return BLOCK_PLANKS;
	};
	blockBehaviors[BLOCK_STAIRS_COBBLESTONE].idDropped = [](uint8_t, Java::Random&) -> ItemId {
		return BLOCK_COBBLESTONE;
	};

	blockBehaviors[BLOCK_SIGN_STANDING].idDropped = [](uint8_t, Java::Random&) -> ItemId {
		return Items::Id::SIGN;
	};
	blockBehaviors[BLOCK_SIGN_WALL].idDropped = [](uint8_t, Java::Random&) -> ItemId {
		return Items::Id::SIGN;
	};
	blockBehaviors[BLOCK_FURNACE_LIT].idDropped = [](uint8_t, Java::Random&) -> ItemId {
		return BLOCK_FURNACE;
	};
	blockBehaviors[BLOCK_REDSTONE_REPEATER_OFF].idDropped = [](uint8_t, Java::Random&) -> ItemId {
		return Items::Id::REDSTONE_REPEATER;
	};
	blockBehaviors[BLOCK_REDSTONE_REPEATER_ON].idDropped = [](uint8_t, Java::Random&) -> ItemId {
		return Items::Id::REDSTONE_REPEATER;
	};
	blockBehaviors[BLOCK_REDSTONE_TORCH_OFF].idDropped = [](uint8_t, Java::Random&) -> ItemId {
		return BLOCK_REDSTONE_TORCH_ON;
	};

	// --------------- drop themselves but pass their metadata onto the item ---------------
	blockBehaviors[BLOCK_WOOL].damageDropped = [](uint8_t _meta) -> ItemDamage {
		return _meta;
	};
	blockBehaviors[BLOCK_LOG].damageDropped = [](uint8_t _meta) -> ItemDamage {
		return _meta;
	};
	blockBehaviors[BLOCK_SAPLING].damageDropped = [](uint8_t _meta) -> ItemDamage {
		return _meta & 3;
	};

	// --------------- don't drop anything ---------------
	blockBehaviors[BLOCK_ICE].quantityDropped = [](Java::Random&) -> ItemAmount {
		return 0;
	};
	blockBehaviors[BLOCK_GLASS].quantityDropped = [](Java::Random&) -> ItemAmount {
		return 0;
	};
	blockBehaviors[BLOCK_BOOKSHELF].quantityDropped = [](Java::Random&) -> ItemAmount {
		return 0;
	};
	blockBehaviors[BLOCK_CAKE].quantityDropped = [](Java::Random&) -> ItemAmount {
		return 0;
	};
	blockBehaviors[BLOCK_MOB_SPAWNER].quantityDropped = [](Java::Random&) -> ItemAmount {
		return 0;
	};
	blockBehaviors[BLOCK_FIRE].quantityDropped = [](Java::Random&) -> ItemAmount {
		return 0;
	};
	blockBehaviors[BLOCK_PISTON_HEAD].quantityDropped = [](Java::Random&) -> ItemAmount {
		return 0;
	};
	blockBehaviors[BLOCK_PISTON_MOVING].quantityDropped = [](Java::Random&) -> ItemAmount {
		return 0;
	};
	blockBehaviors[BLOCK_NETHER_PORTAL].quantityDropped = [](Java::Random&) -> ItemAmount {
		return 0;
	};
	blockBehaviors[BLOCK_SNOW_LAYER].quantityDropped = [](Java::Random&) -> ItemAmount {
		return 0;
	};
	blockBehaviors[BLOCK_WATER_FLOWING].quantityDropped = [](Java::Random&) -> ItemAmount {
		return 0;
	};
	blockBehaviors[BLOCK_WATER_STILL].quantityDropped = [](Java::Random&) -> ItemAmount {
		return 0;
	};
	blockBehaviors[BLOCK_LAVA_FLOWING].quantityDropped = [](Java::Random&) -> ItemAmount {
		return 0;
	};
	blockBehaviors[BLOCK_LAVA_STILL].quantityDropped = [](Java::Random&) -> ItemAmount {
		return 0;
	};

	// --------------- drops influenced by RNG ---------------
	blockBehaviors[BLOCK_GRAVEL].idDropped = [](uint8_t, Java::Random& _rng) -> ItemId {
		return _rng.NextInt(10) == 0 ? static_cast<ItemId>(Items::Id::FLINT) : static_cast<ItemId>(BLOCK_GRAVEL);
	};

	blockBehaviors[BLOCK_ORE_LAPIS_LAZULI].idDropped = [](uint8_t, Java::Random&) -> ItemId {
		return Items::Id::DYE;
	};
	blockBehaviors[BLOCK_ORE_LAPIS_LAZULI].damageDropped = [](uint8_t) -> ItemDamage {
		return 4;
	};
	blockBehaviors[BLOCK_ORE_LAPIS_LAZULI].quantityDropped = [](Java::Random& _rng) -> ItemAmount {
		return 4 + _rng.NextInt(5);
	};

	blockBehaviors[BLOCK_ORE_REDSTONE_OFF].idDropped = [](uint8_t, Java::Random&) -> ItemId {
		return Items::Id::REDSTONE;
	};
	blockBehaviors[BLOCK_ORE_REDSTONE_OFF].quantityDropped = [](Java::Random& _rng) -> ItemAmount {
		return 4 + _rng.NextInt(2);
	};
	blockBehaviors[BLOCK_ORE_REDSTONE_ON].idDropped = [](uint8_t, Java::Random&) -> ItemId {
		return Items::Id::REDSTONE;
	};
	blockBehaviors[BLOCK_ORE_REDSTONE_ON].quantityDropped = [](Java::Random& _rng) -> ItemAmount {
		return 4 + _rng.NextInt(2);
	};

	blockBehaviors[BLOCK_GLOWSTONE].idDropped = [](uint8_t, Java::Random&) -> ItemId {
		return Items::Id::GLOWSTONE_DUST;
	};
	blockBehaviors[BLOCK_GLOWSTONE].quantityDropped = [](Java::Random& _rng) -> ItemAmount {
		return 2 + _rng.NextInt(3);
	};
	blockBehaviors[BLOCK_LEAVES].idDropped = [](uint8_t, Java::Random&) -> ItemId {
		return BLOCK_SAPLING;
	};
	blockBehaviors[BLOCK_LEAVES].damageDropped = [](uint8_t _meta) -> ItemDamage {
		return _meta & 3;
	};
	blockBehaviors[BLOCK_LEAVES].quantityDropped = [](Java::Random& _rng) -> ItemAmount {
		return _rng.NextInt(20) == 0 ? 1 : 0;
	};

	blockBehaviors[BLOCK_TALLGRASS].idDropped = [](uint8_t, Java::Random& _rng) -> ItemId {
		return _rng.NextInt(8) == 0 ? Items::Id::SEEDS_WHEAT : Items::Id::INVALID;
	};

	// --------------- only drop if it's the correct half of the block being broken ---------------
	// TODO: other half of the block should be removed automatically
	blockBehaviors[BLOCK_DOOR_WOOD].idDropped = [](uint8_t _meta, Java::Random&) -> ItemId {
		return (_meta & 8) != 0 ? Items::Id::INVALID : Items::Id::DOOR_WOOD;
	};

	blockBehaviors[BLOCK_DOOR_IRON].idDropped = [](uint8_t _meta, Java::Random&) -> ItemId {
		return (_meta & 8) != 0 ? Items::Id::INVALID : Items::Id::DOOR_IRON;
	};

	blockBehaviors[BLOCK_BED].idDropped = [](uint8_t _meta, Java::Random&) -> ItemId {
		return (_meta & 8) != 0 ? Items::Id::INVALID : Items::Id::BED;
	};

	blockBehaviors[BLOCK_CLAY].idDropped = [](uint8_t, Java::Random&) -> ItemId {
		return Items::Id::CLAY;
	};
	blockBehaviors[BLOCK_CLAY].quantityDropped = [](Java::Random&) -> ItemAmount {
		return 4;
	};

	blockBehaviors[BLOCK_SLAB].damageDropped = [](uint8_t _meta) -> ItemDamage {
		return _meta;
	};

	blockBehaviors[BLOCK_DOUBLE_SLAB].idDropped = [](uint8_t, Java::Random&) -> ItemId {
		return BLOCK_SLAB;
	};
	blockBehaviors[BLOCK_DOUBLE_SLAB].damageDropped = [](uint8_t _meta) -> ItemDamage {
		return _meta;
	};
	blockBehaviors[BLOCK_DOUBLE_SLAB].quantityDropped = [](Java::Random&) -> ItemAmount {
		return 2;
	};

	blockBehaviors[BLOCK_SNOW].idDropped = [](uint8_t, Java::Random&) -> ItemId {
		return Items::Id::SNOWBALL;
	};
	blockBehaviors[BLOCK_SNOW].quantityDropped = [](Java::Random&) -> ItemAmount {
		return 4;
	};
}

std::vector<ItemStack> GetBlockDrops(BlockType _blockId, uint8_t _meta, Java::Random& _rng) {
	std::vector<ItemStack> drops;

	if (_blockId == BLOCK_AIR) {
		return drops;
	}

	// headache: crops drop multiple items of different types (wheat + seeds)
	if (_blockId == BLOCK_CROP_WHEAT) {
		if (_meta == MAX_CROP_SIZE) {
			drops.push_back(ItemStack{ Items::Id::WHEAT, 1, 0 });
		}

		for (int i = 0; i < 3; i++) {
			if (_rng.NextInt(15) <= static_cast<int>(_meta)) {
				drops.push_back(ItemStack{ Items::Id::SEEDS_WHEAT, 1, 0 });
			}
		}

		return drops;
	}

	const BlockBehavior& behavior = blockBehaviors[static_cast<uint8_t>(_blockId)];
	int count = behavior.quantityDropped ? behavior.quantityDropped(_rng) : 1;
	int16_t damage = behavior.damageDropped ? behavior.damageDropped(_meta) : 0;

	for (int i = 0; i < count; i++) {
		ItemId id = behavior.idDropped ? behavior.idDropped(_meta, _rng) : ItemId(_blockId);

		if (id > 0) {
			drops.push_back(ItemStack{ id, 1, damage });
		}
	}

	return drops;
}

}; // namespace Blocks
