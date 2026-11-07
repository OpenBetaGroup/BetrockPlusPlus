/*
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 * 
*/

#pragma once

#include "hash.h"
#include "inventory/item_stack.h"
#include "numeric_structs.h"

#include <array>
#include <span>
#include <unordered_map>
#include <utility>

struct ItemKey {
	ItemId id = Items::Id::INVALID;
	ItemDamage data = 0;
	ItemStack transformTo{}; // Empty slot by default

	bool operator<(const ItemKey& _other) const {
		return id != _other.id ? id < _other.id : data < _other.data;
	}

	bool operator==(const ItemKey& _other) const {
		return id == _other.id && data == _other.data;
	}
};

struct ShapedRecipeKey {
	uint8_t width = 0;
	uint8_t height = 0;
	std::array<ItemKey, 9> cells{};

	friend bool operator==(const ShapedRecipeKey&, const ShapedRecipeKey&) = default;
};

struct ShapelessRecipeKey {
	uint8_t count = 0;
	std::array<ItemKey, 9> items{};

	friend bool operator==(const ShapelessRecipeKey&, const ShapelessRecipeKey&) = default;
};

namespace std {
template <>
struct hash<ItemKey> {
	size_t operator()(const ItemKey& _k) const noexcept {
		size_t h = 0;
		HashCombine(h, _k.id);
		HashCombine(h, _k.data);
		return h;
	}
};
} // namespace std

struct ShapedRecipeKeyHasher {
	size_t operator()(const ShapedRecipeKey& _key) const {
		size_t h = 0;

		HashCombine(h, _key.width);
		HashCombine(h, _key.height);

		for (const auto& cell : _key.cells)
			HashCombine(h, cell);

		return h;
	}
};

struct ShapelessRecipeKeyHasher {
	size_t operator()(const ShapelessRecipeKey& _key) const {
		size_t h = 0;

		HashCombine(h, _key.count);

		for (const auto& item : _key.items)
			HashCombine(h, item);

		return h;
	}
};

class RecipeManager {
public:
	void AddShapedRecipe(std::initializer_list<std::string_view> _rows,
	                     std::initializer_list<std::pair<char, ItemKey>> _mapping, ItemStack _output);
	void AddShapelessRecipe(std::span<const ItemKey> _items, ItemStack _output);

	void AddVanillaRecipes();

	[[nodiscard]] const std::pair<const std::array<ItemKey, 9>, ItemStack> MatchGrid(std::span<const ItemStack> _grid,
	                                                                                  UInt8_2 _size) const;

private:
	static ShapedRecipeKey MakeShapedKey(std::span<const ItemKey> _grid, UInt8_2 _size);
	static ShapelessRecipeKey MakeShapelessKey(std::span<const ItemKey> _items);

	std::unordered_map<ShapedRecipeKey, ItemStack, ShapedRecipeKeyHasher> shapedRecipes;
	std::unordered_map<ShapelessRecipeKey, ItemStack, ShapelessRecipeKeyHasher> shapelessRecipes;
};