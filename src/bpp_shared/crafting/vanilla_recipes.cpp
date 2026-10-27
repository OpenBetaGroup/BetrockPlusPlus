/*
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 * 
*/

#include "enums/blocks.h"
#include "enums/items.h"
#include "recipe_manager.h"

void RecipeManager::AddVanillaRecipes() {
	// Msc
	AddShapedRecipe({ "###", "# #", "###" }, { { '#', { BLOCK_PLANKS } } }, { BLOCK_CHEST, 1 });
	AddShapedRecipe({ "###", "# #", "###" }, { { '#', { BLOCK_COBBLESTONE } } }, { BLOCK_FURNACE, 1 });
	AddShapedRecipe({ "##", "##" }, { { '#', { BLOCK_PLANKS } } }, { BLOCK_CRAFTING_TABLE, 1 });
	AddShapedRecipe({ "#", "#" }, { { '#', { BLOCK_PLANKS } } }, { Items::Id::STICK, 4 });
	AddShapedRecipe({ "#" }, { { '#', { BLOCK_LOG } } }, { BLOCK_PLANKS, 4 });
	AddShapedRecipe({ "###" }, { { '#', { Items::Id::SUGARCANE } } }, { Items::Id::PAPER, 3 });
	AddShapedRecipe({ "#", "#", "#" }, { { '#', { Items::Id::PAPER } } }, { Items::Id::BOOK, 1 });
	AddShapedRecipe({ "###", "#X#", "###" }, { { '#', { BLOCK_PLANKS } }, { 'X', { Items::Id::DIAMOND } } },
	                { BLOCK_JUKEBOX, 1 });
	AddShapedRecipe({ "###", "#X#", "###" }, { { '#', { BLOCK_PLANKS } }, { 'X', { Items::Id::REDSTONE } } },
	                { BLOCK_NOTEBLOCK, 1 });
	AddShapedRecipe({ "###", "XXX", "###" }, { { '#', { BLOCK_PLANKS } }, { 'X', { Items::Id::BOOK } } },
	                { BLOCK_BOOKSHELF, 1 });
	AddShapedRecipe({ "##", "##" }, { { '#', { Items::Id::SNOWBALL } } }, { BLOCK_SNOW, 1 });
	AddShapedRecipe({ "##", "##" }, { { '#', { Items::Id::CLAY } } }, { BLOCK_CLAY, 1 });
	AddShapedRecipe({ "##", "##" }, { { '#', { Items::Id::BRICK } } }, { BLOCK_BRICKS, 1 });
	AddShapedRecipe({ "##", "##" }, { { '#', { Items::Id::GLOWSTONE_DUST } } }, { BLOCK_GLOWSTONE, 1 });
	AddShapedRecipe({ "##", "##" }, { { '#', { Items::Id::STRING } } }, { BLOCK_WOOL, 1 });
	AddShapedRecipe({ "##", "##" }, { { '#', { BLOCK_SAND } } }, { BLOCK_SANDSTONE, 1 });
	AddShapedRecipe({ "X#X", "#X#", "X#X" }, { { '#', { BLOCK_SAND } }, { 'X', { Items::Id::GUNPOWDER } } },
	                { BLOCK_TNT, 1 });
	AddShapedRecipe({ "#  ", "## ", "###" }, { { '#', { BLOCK_PLANKS } } }, { BLOCK_STAIRS_WOOD, 4 });
	AddShapedRecipe({ "#  ", "## ", "###" }, { { '#', { BLOCK_COBBLESTONE } } }, { BLOCK_STAIRS_COBBLESTONE, 4 });
	AddShapedRecipe({ "###" }, { { '#', { BLOCK_PLANKS } } }, { BLOCK_SLAB, 3, 2 });
	AddShapedRecipe({ "###" }, { { '#', { BLOCK_COBBLESTONE } } }, { BLOCK_SLAB, 3, 3 });
	AddShapedRecipe({ "###" }, { { '#', { BLOCK_STONE } } }, { BLOCK_SLAB, 3, 0 });
	AddShapedRecipe({ "###" }, { { '#', { BLOCK_SANDSTONE } } }, { BLOCK_SLAB, 3, 1 });
	AddShapedRecipe({ "###", "###" }, { { '#', { Items::Id::STICK } } }, { BLOCK_FENCE, 2 });
	AddShapedRecipe({ "# #", "###", "# #" }, { { '#', { Items::Id::STICK } } }, { BLOCK_LADDER, 2 });
	AddShapedRecipe({ "##", "##", "##" }, { { '#', { BLOCK_PLANKS } } }, { Items::Id::DOOR_WOOD, 1 });
	AddShapedRecipe({ "##", "##", "##" }, { { '#', { Items::Id::IRON } } }, { Items::Id::DOOR_IRON, 1 });
	AddShapedRecipe({ "# ", " #" }, { { '#', { Items::Id::IRON } } }, { Items::Id::SHEARS, 1 });
	AddShapedRecipe({ "###", "###", " X " }, { { '#', { BLOCK_PLANKS } }, { 'X', { Items::Id::STICK } } },
	                { Items::Id::SIGN, 1 });
	AddShapedRecipe({ "AAA", "BEB", "CCC" },
	                { { 'A', { .id = Items::Id::BUCKET_MILK, .transformTo = { Items::Id::BUCKET, 1 } } },
	                  { 'B', { Items::Id::SUGAR } },
	                  { 'C', { Items::Id::WHEAT } },
	                  { 'E', { Items::Id::EGG } } },
	                { Items::Id::CAKE, 1 });
	AddShapedRecipe({ "#" }, { { '#', { Items::Id::SUGARCANE } } }, { Items::Id::SUGAR, 1 });
	AddShapedRecipe({ "#", "X" }, { { '#', { Items::Id::COAL } }, { 'X', { Items::Id::STICK } } }, { BLOCK_TORCH, 4 });
	AddShapedRecipe({ "#", "X" }, { { '#', { Items::Id::COAL, 1 } }, { 'X', { Items::Id::STICK } } },
	                { BLOCK_TORCH, 4 });
	AddShapedRecipe({ "# #", " # " }, { { '#', { BLOCK_PLANKS } } }, { Items::Id::BOWL, 4 });
	AddShapedRecipe({ "X X", "X#X", "X X" }, { { 'X', { Items::Id::IRON } }, { '#', { Items::Id::STICK } } },
	                { BLOCK_RAIL, 16 });
	AddShapedRecipe({ "X X", "X#X", "XRX" },
	                { { 'X', { Items::Id::GOLD } }, { '#', { Items::Id::STICK } }, { 'R', { Items::Id::REDSTONE } } },
	                { BLOCK_RAIL_POWERED, 6 });
	AddShapedRecipe(
	    { "X X", "X#X", "XRX" },
	    { { 'X', { Items::Id::IRON } }, { '#', { BLOCK_PRESSURE_PLATE_STONE } }, { 'R', { Items::Id::REDSTONE } } },
	    { BLOCK_RAIL_DETECTOR, 6 });
	AddShapedRecipe({ "# #", "###" }, { { '#', { Items::Id::IRON } } }, { Items::Id::MINECART, 1 });
	AddShapedRecipe({ "#", "A" }, { { '#', { BLOCK_PUMPKIN } }, { 'A', { BLOCK_TORCH } } }, { BLOCK_PUMPKIN_LIT, 1 });
	AddShapedRecipe({ "#", "A" }, { { '#', { BLOCK_CHEST } }, { 'A', { Items::Id::MINECART } } },
	                { Items::Id::MINECART_CHEST, 1 });
	AddShapedRecipe({ "#", "A" }, { { '#', { BLOCK_FURNACE } }, { 'A', { Items::Id::MINECART } } },
	                { Items::Id::MINECART_FURNACE, 1 });
	AddShapedRecipe({ "# #", "###" }, { { '#', { BLOCK_PLANKS } } }, { Items::Id::BOAT, 1 });
	AddShapedRecipe({ "# #", " # " }, { { '#', { Items::Id::IRON } } }, { Items::Id::BUCKET, 1 });
	AddShapedRecipe({ "A ", " B" }, { { 'A', { Items::Id::IRON } }, { 'B', { Items::Id::FLINT } } },
	                { Items::Id::FLINT_AND_STEEL, 1 });
	AddShapedRecipe({ " A", "B " }, { { 'A', { Items::Id::IRON } }, { 'B', { Items::Id::FLINT } } },
	                { Items::Id::FLINT_AND_STEEL, 1 });
	AddShapedRecipe({ "###" }, { { '#', { Items::Id::WHEAT } } }, { Items::Id::BREAD, 1 });
	AddShapedRecipe({ "  #", " #X", "# X" }, { { '#', { Items::Id::STICK } }, { 'X', { Items::Id::STRING } } },
	                { Items::Id::FISHING_ROD, 1 });
	AddShapedRecipe({ "###", "#X#", "###" }, { { '#', { Items::Id::STICK } }, { 'X', { BLOCK_WOOL } } },
	                { Items::Id::PAINTING, 1 });
	AddShapedRecipe({ "###", "#X#", "###" }, { { '#', { BLOCK_GOLD } }, { 'X', { Items::Id::APPLE } } },
	                { Items::Id::APPLE_GOLDEN, 1 });
	AddShapedRecipe({ "X", "#" }, { { 'X', { Items::Id::STICK } }, { '#', { BLOCK_COBBLESTONE } } }, { BLOCK_LEVER, 1 });
	AddShapedRecipe({ "X", "#" }, { { 'X', { Items::Id::REDSTONE } }, { '#', { Items::Id::STICK } } },
	                { BLOCK_REDSTONE_TORCH_ON, 1 });
	AddShapedRecipe({ "#X#", "III" },
	                { { '#', { BLOCK_REDSTONE_TORCH_ON } }, { 'X', { Items::Id::REDSTONE } }, { 'I', { BLOCK_STONE } } },
	                { Items::Id::REDSTONE_REPEATER, 1 });
	AddShapedRecipe({ " # ", "#X#", " # " }, { { '#', { Items::Id::GOLD } }, { 'X', { Items::Id::REDSTONE } } },
	                { Items::Id::CLOCK, 1 });
	AddShapedRecipe({ " # ", "#X#", " # " }, { { '#', { Items::Id::IRON } }, { 'X', { Items::Id::REDSTONE } } },
	                { Items::Id::COMPASS, 1 });
#ifdef EXPERIMENTAL
	AddShapedRecipe({ "###", "#X#", "###" }, { { '#', { Items::Id::PAPER } }, { 'X', { Items::Id::COMPASS } } },
	                { Items::Id::MAP, 1 });
#endif
	AddShapedRecipe({ "#", "#" }, { { '#', { BLOCK_STONE } } }, { BLOCK_BUTTON_STONE, 1 });
	AddShapedRecipe({ "##" }, { { '#', { BLOCK_STONE } } }, { BLOCK_PRESSURE_PLATE_STONE, 1 });
	AddShapedRecipe({ "##" }, { { '#', { BLOCK_PLANKS } } }, { BLOCK_PRESSURE_PLATE_WOOD, 1 });
	AddShapedRecipe({ "###", "#X#", "#R#" },
	                { { '#', { BLOCK_COBBLESTONE } }, { 'X', { Items::Id::BOW } }, { 'R', { Items::Id::REDSTONE } } },
	                { BLOCK_DISPENSER, 1 });
	AddShapedRecipe({ "TTT", "#X#", "#R#" },
	                { { '#', { BLOCK_COBBLESTONE } },
	                  { 'X', { Items::Id::IRON } },
	                  { 'R', { Items::Id::REDSTONE } },
	                  { 'T', { BLOCK_PLANKS } } },
	                { BLOCK_PISTON, 1 });
	AddShapedRecipe({ "S", "P" }, { { 'S', { Items::Id::SLIME } }, { 'P', { BLOCK_PISTON } } },
	                { BLOCK_PISTON_STICKY, 1 });
	AddShapedRecipe({ "###", "XXX" }, { { '#', { BLOCK_WOOL } }, { 'X', { BLOCK_PLANKS } } }, { Items::Id::BED, 1 });
	AddShapedRecipe({ "###", "###" }, { { '#', { BLOCK_PLANKS } } }, { BLOCK_TRAPDOOR, 2 });

	// Armor
	auto addArmor = [this](ItemId _material, ItemId _helmetId, ItemId _chestId, ItemId _leggingsId,
	                       ItemId _bootsId) -> void {
		AddShapedRecipe({ "###", "# #" }, { { '#', { _material } } }, { _helmetId, 1 });
		AddShapedRecipe({ "# #", "###", "###" }, { { '#', { _material } } }, { _chestId, 1 });
		AddShapedRecipe({ "###", "# #", "# #" }, { { '#', { _material } } }, { _leggingsId, 1 });
		AddShapedRecipe({ "# #", "# #" }, { { '#', { _material } } }, { _bootsId, 1 });
	};

	addArmor(Items::Id::GOLD, Items::Id::HELMET_GOLD, Items::Id::CHESTPLATE_GOLD, Items::Id::LEGGINGS_GOLD,
	         Items::Id::BOOTS_GOLD);
	addArmor(Items::Id::IRON, Items::Id::HELMET_IRON, Items::Id::CHESTPLATE_IRON, Items::Id::LEGGINGS_IRON,
	         Items::Id::BOOTS_IRON);
	addArmor(Items::Id::DIAMOND, Items::Id::HELMET_DIAMOND, Items::Id::CHESTPLATE_DIAMOND, Items::Id::LEGGINGS_DIAMOND,
	         Items::Id::BOOTS_DIAMOND);
	addArmor(BLOCK_FIRE, Items::Id::HELMET_CHAINMAIL, Items::Id::CHESTPLATE_CHAINMAIL, Items::Id::LEGGINGS_CHAINMAIL,
	         Items::Id::BOOTS_CHAINMAIL);
	addArmor(Items::Id::LEATHER, Items::Id::HELMET_LEATHER, Items::Id::CHESTPLATE_LEATHER, Items::Id::LEGGINGS_LEATHER,
	         Items::Id::BOOTS_LEATHER);

	// Tools
	auto addTools = [this](ItemId _toolMaterial, ItemId _swordId, ItemId _pickId, ItemId _shovelId, ItemId _axeId,
	                       ItemId _hoeId) -> void {
		AddShapedRecipe({ "###", " A ", " A " }, { { '#', { _toolMaterial } }, { 'A', { Items::Id::STICK } } },
		                { _pickId, 1 });
		AddShapedRecipe({ "#", "#", "A" }, { { '#', { _toolMaterial } }, { 'A', { Items::Id::STICK } } },
		                { _swordId, 1 });
		AddShapedRecipe({ "## ", "#A ", " A " }, { { '#', { _toolMaterial } }, { 'A', { Items::Id::STICK } } },
		                { _axeId, 1 });
		AddShapedRecipe({ " ##", " A#", " A " }, { { '#', { _toolMaterial } }, { 'A', { Items::Id::STICK } } },
		                { _axeId, 1 });
		AddShapedRecipe({ "#", "A", "A" }, { { '#', { _toolMaterial } }, { 'A', { Items::Id::STICK } } },
		                { _shovelId, 1 });
		AddShapedRecipe({ "## ", " A ", " A " }, { { '#', { _toolMaterial } }, { 'A', { Items::Id::STICK } } },
		                { _hoeId, 1 });
		AddShapedRecipe({ " ##", " A ", " A " }, { { '#', { _toolMaterial } }, { 'A', { Items::Id::STICK } } },
		                { _hoeId, 1 });
	};

	addTools(BLOCK_COBBLESTONE, Items::Id::SWORD_STONE, Items::Id::PICKAXE_STONE, Items::Id::SHOVEL_STONE,
	         Items::Id::AXE_STONE, Items::Id::HOE_STONE);
	addTools(BLOCK_PLANKS, Items::Id::SWORD_WOOD, Items::Id::PICKAXE_WOOD, Items::Id::SHOVEL_WOOD, Items::Id::AXE_WOOD,
	         Items::Id::HOE_WOOD);
	addTools(Items::Id::IRON, Items::Id::SWORD_IRON, Items::Id::PICKAXE_IRON, Items::Id::SHOVEL_IRON,
	         Items::Id::AXE_IRON, Items::Id::HOE_IRON);
	addTools(Items::Id::GOLD, Items::Id::SWORD_GOLD, Items::Id::PICKAXE_GOLD, Items::Id::SHOVEL_GOLD,
	         Items::Id::AXE_GOLD, Items::Id::HOE_GOLD);
	addTools(Items::Id::DIAMOND, Items::Id::SWORD_DIAMOND, Items::Id::PICKAXE_DIAMOND, Items::Id::SHOVEL_DIAMOND,
	         Items::Id::AXE_DIAMOND, Items::Id::HOE_DIAMOND);

	// Bows and arrows
	AddShapedRecipe({ " #A", "# A", " #A" }, { { '#', { Items::Id::STICK } }, { 'A', { Items::Id::STRING } } },
	                { Items::Id::BOW, 1 });
	AddShapedRecipe({ "A", "B", "C" },
	                { { 'A', { Items::Id::FLINT } }, { 'B', { Items::Id::STICK } }, { 'C', { Items::Id::FEATHER } } },
	                { Items::Id::ARROW, 4 });

	// Blocks -> ingots, ingots -> blocks
	auto addMaterial = [this](ItemId _material, uint8_t _materialMeta, ItemId _storedMaterial) -> void {
		AddShapedRecipe({ "###", "###", "###" }, { { '#', { _material, _materialMeta } } }, { _storedMaterial, 1 });
		AddShapedRecipe({ "#" }, { { '#', { _storedMaterial } } }, { _material, 9, _materialMeta });
	};

	addMaterial(Items::Id::IRON, 0, BLOCK_IRON);
	addMaterial(Items::Id::DIAMOND, 0, BLOCK_DIAMOND);
	addMaterial(Items::Id::GOLD, 0, BLOCK_GOLD);
	addMaterial(Items::Id::DYE, 4, BLOCK_LAPIS_LAZULI);

	// Food items
	AddShapedRecipe({ "Y", "X", "#" },
	                { { 'X', { BLOCK_MUSHROOM_BROWN } }, { 'Y', { BLOCK_MUSHROOM_RED } }, { '#', { Items::Id::BOWL } } },
	                { Items::Id::MUSHROOM_STEW, 1 });
	AddShapedRecipe({ "Y", "X", "#" },
	                { { 'X', { BLOCK_MUSHROOM_RED } }, { 'Y', { BLOCK_MUSHROOM_BROWN } }, { '#', { Items::Id::BOWL } } },
	                { Items::Id::MUSHROOM_STEW, 1 });
	AddShapedRecipe({ "#X#" }, { { 'X', { Items::Id::DYE, 3 } }, { '#', { Items::Id::WHEAT } } },
	                { Items::Id::COOKIE, 8 });

	// Wool + Dye -> redyed Wool (dye meta and wool meta are inverse: 15 - dyeMeta)
	for (uint8_t i = 0; i < 16; ++i) {
		AddShapelessRecipe(std::to_array<ItemKey>({ { Items::Id::DYE, i }, { BLOCK_WOOL, 0 } }),
		                   { BLOCK_WOOL, 1, static_cast<ItemDamage>(15 - i) });
	}

	// Dye meta reference: 0=Black 1=Red 2=Green 3=Brown 4=Blue 5=Purple 6=Cyan
	//                     7=LightGray 8=Gray 9=Pink 10=Lime 11=Yellow 12=LightBlue
	//                     13=Magenta 14=Orange 15=White

	AddShapelessRecipe(std::to_array<ItemKey>({ { BLOCK_DANDELION } }),
	                   { Items::Id::DYE, 2, 11 });                                            // Dandelion -> Yellow
	AddShapelessRecipe(std::to_array<ItemKey>({ { BLOCK_ROSE } }), { Items::Id::DYE, 2, 1 }); // Rose -> Red
	AddShapelessRecipe(std::to_array<ItemKey>({ { Items::Id::BONE } }),
	                   { Items::Id::DYE, 3, 15 }); // Bone -> Bone Meal (White)

	AddShapelessRecipe(std::to_array<ItemKey>({ { Items::Id::DYE, 1 }, { Items::Id::DYE, 15 } }),
	                   { Items::Id::DYE, 2, 9 }); // Red + White -> Pink
	AddShapelessRecipe(std::to_array<ItemKey>({ { Items::Id::DYE, 1 }, { Items::Id::DYE, 11 } }),
	                   { Items::Id::DYE, 2, 14 }); // Red + Yellow -> Orange
	AddShapelessRecipe(std::to_array<ItemKey>({ { Items::Id::DYE, 2 }, { Items::Id::DYE, 15 } }),
	                   { Items::Id::DYE, 2, 10 }); // Green + White -> Lime
	AddShapelessRecipe(std::to_array<ItemKey>({ { Items::Id::DYE, 0 }, { Items::Id::DYE, 15 } }),
	                   { Items::Id::DYE, 2, 8 }); // Black + White -> Gray
	AddShapelessRecipe(std::to_array<ItemKey>({ { Items::Id::DYE, 8 }, { Items::Id::DYE, 15 } }),
	                   { Items::Id::DYE, 2, 7 }); // Gray + White -> Light Gray
	AddShapelessRecipe(std::to_array<ItemKey>({ { Items::Id::DYE, 0 }, { Items::Id::DYE, 15 }, { Items::Id::DYE, 15 } }),
	                   { Items::Id::DYE, 3, 7 }); // Black + White + White -> Light Gray (alt)
	AddShapelessRecipe(std::to_array<ItemKey>({ { Items::Id::DYE, 4 }, { Items::Id::DYE, 15 } }),
	                   { Items::Id::DYE, 2, 12 }); // Blue + White -> Light Blue
	AddShapelessRecipe(std::to_array<ItemKey>({ { Items::Id::DYE, 4 }, { Items::Id::DYE, 2 } }),
	                   { Items::Id::DYE, 2, 6 }); // Blue + Green -> Cyan
	AddShapelessRecipe(std::to_array<ItemKey>({ { Items::Id::DYE, 4 }, { Items::Id::DYE, 1 } }),
	                   { Items::Id::DYE, 2, 5 }); // Blue + Red -> Purple
	AddShapelessRecipe(std::to_array<ItemKey>({ { Items::Id::DYE, 5 }, { Items::Id::DYE, 9 } }),
	                   { Items::Id::DYE, 2, 13 }); // Purple + Pink -> Magenta
	AddShapelessRecipe(std::to_array<ItemKey>({ { Items::Id::DYE, 4 }, { Items::Id::DYE, 1 }, { Items::Id::DYE, 9 } }),
	                   { Items::Id::DYE, 3, 13 }); // Blue + Red + Pink -> Magenta (alt)
	AddShapelessRecipe(
	    std::to_array<ItemKey>(
	        { { Items::Id::DYE, 4 }, { Items::Id::DYE, 1 }, { Items::Id::DYE, 1 }, { Items::Id::DYE, 15 } }),
	    { Items::Id::DYE, 4, 13 }); // Blue + Red + Red + White -> Magenta (alt 2)
}