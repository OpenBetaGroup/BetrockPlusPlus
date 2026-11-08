/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "tile_entity_furnace.h"
#include "world/world.h"

void TileEntityFurnace::Deserialize(const Tag& _tag) {
	burnTime = _tag.Has("BurnTime") ? _tag.Get("BurnTime").GetShort() : 0;
	maxBurnTime = _tag.Has("MaxBurnTime") ? _tag.Get("MaxBurnTime").GetShort() : 0;
	cookTime = _tag.Has("CookTime") ? _tag.Get("CookTime").GetShort() : 0;
	LoadSlots(_tag, this->inventory.slots);
}

static ItemStack GetSmeltingResult(ItemId& _input) {
	switch (_input) {
	case BLOCK_ORE_IRON:
		return { Items::IRON, 1, 0 };
	case BLOCK_ORE_GOLD:
		return { Items::GOLD, 1, 0 };
	case BLOCK_ORE_DIAMOND:
		return { Items::DIAMOND, 1, 0 };
	case BLOCK_SAND:
		return { BLOCK_GLASS, 1, 0 };
	case Items::PORKCHOP:
		return { Items::PORKCHOP_COOKED, 1, 0 };
	case Items::FISH:
		return { Items::FISH_COOKED, 1, 0 };
	case BLOCK_COBBLESTONE:
		return { BLOCK_STONE, 1, 0 };
	case Items::CLAY:
		return { Items::BRICK, 1, 0 };
	case BLOCK_CACTUS:
		return { Items::DYE, 1, 2 }; // Green dye
	case BLOCK_LOG:
		return { Items::COAL, 1, 1 }; // Charcoal
	}

	return ItemStack{};
}

static bool CanAcceptSmeltResult(Inventory& _inventory, const ItemStack& _result) {
	ItemStack& output = _inventory.slots[2];

	if (output.id == Items::INVALID)
		return true;
	if (output.id != _result.id || output.data != _result.data)
		return false;

	int maxStack = Items::GetMaxStack(output.id);
	return (output.count + _result.count) <= maxStack;
}

void TileEntityFurnace::Tick(WorldManager& _world) {
	dirtyFlags = FLAG_NONE;

	bool hasInput = inventory.slots[0].id != Items::INVALID && inventory.slots[0].count > 0;
	ItemStack result = hasInput ? GetSmeltingResult(inventory.slots[0].id) : ItemStack{ Items::INVALID };
	bool canSmelt = hasInput && result.id != Items::INVALID && CanAcceptSmeltResult(inventory, result);

	bool wasBurning = burnTime > 0;

	{
		const int oldBurnTime = burnTime;

		if (burnTime > 0)
			--burnTime;

		if (burnTime == 0 && canSmelt) {
			const int fuelTime = CalculateFuelTime();
			if (fuelTime != maxBurnTime && fuelTime != 0) {
				dirtyFlags |= FLAG_MAX_BURN_TIME;
				maxBurnTime = fuelTime;
			}

			burnTime = fuelTime;
			if (burnTime > 0)
				inventory.DecreaseStackSize(1, 1);
		}

		if (burnTime != oldBurnTime)
			dirtyFlags |= FLAG_BURN_TIME;
	}

	if (wasBurning != (burnTime > 0)) {
		// Flip furnace block ID
		auto oldMeta = _world.GetMetadata(this->position);
		_world.SetBlock(this->position, BlockType(61 + (burnTime > 0)), oldMeta, /*Keep Tile Entity=*/true);
	}

	{
		const int oldCookTime = cookTime;

		if (burnTime > 0 && canSmelt) {
			if (++cookTime >= 200) {
				inventory.MergeItemStackInInventory(result, false, 2, 2);
				inventory.DecreaseStackSize(0, 1);
				cookTime = 0;
			}
		} else {
			cookTime = 0;
		}

		if (cookTime != oldCookTime)
			dirtyFlags |= FLAG_COOK_TIME;
	}

	if (chunk && inventory.isModified) {
		chunk->isModified = true;
		inventory.isModified = false;
	}
}

int TileEntityFurnace::CalculateFuelTime() const {
	const ItemId fuel = inventory.slots[1].id;

	switch (fuel) {
	case Items::COAL:
		return 1600;
	case Items::STICK:
	case BLOCK_SAPLING:
		return 100;
	case Items::BUCKET_LAVA:
		return 20000;
	}

	if (Items::IsBlock(fuel) && Blocks::blockProperties[fuel].material == Material::Wood()) {
		return 300;
	}

	return 0;
}

int TileEntityFurnace::GetBurnTime() const {
	return burnTime;
}

int TileEntityFurnace::GetMaxBurnTime() const {
	return maxBurnTime;
}

int TileEntityFurnace::GetCookTime() const {
	return cookTime;
}

Tag TileEntityFurnace::Serialize() {
	auto root = TileEntity::Serialize();
	SerializeInventory(root, inventory);

	root.compound["BurnTime"] = Tag{ .type = TAG_SHORT,
		                             .name = "BurnTime",
		                             .shortValue = static_cast<int16_t>(burnTime) };

	// Vanilla doesn't save this, but it's better if we do.
	// Prevents a bug where progress bar is displayed wrong after world load
	root.compound["MaxBurnTime"] = Tag{ .type = TAG_SHORT,
		                                .name = "MaxBurnTime",
		                                .shortValue = static_cast<int16_t>(maxBurnTime) };
	root.compound["CookTime"] = Tag{ .type = TAG_SHORT,
		                             .name = "CookTime",
		                             .shortValue = static_cast<int16_t>(cookTime) };

	return root;
}