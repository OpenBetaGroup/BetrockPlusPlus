/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once
#include "../base_types.h"
#include "enums/items.h"
#include "hash.h"
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>

// Just a virtual container
struct ItemStack {
	ItemId id = Items::Id::INVALID;
	ItemAmount count = 0;
	ItemDamage data = 0; // This is "damage" in the og java but data makes more sense for what this is used for

	bool operator==(const ItemStack& _o) const {
		return id == _o.id && count == _o.count && data == _o.data;
	}
	bool operator!=(const ItemStack& _o) const {
		return !(*this == _o);
	}

	friend std::wostream& operator<<(std::wostream& _wos, const ItemStack& _val) {
		_wos << "(" << static_cast<int64_t>(_val.id) << ":" << static_cast<int64_t>(_val.data) << " x"
		     << static_cast<int64_t>(_val.count) << ")";
		return _wos;
	}

	friend std::ostream& operator<<(std::ostream& _os, const ItemStack& _val) {
		_os << "(" << static_cast<int64_t>(_val.id) << ":" << static_cast<int64_t>(_val.data) << " x"
		    << static_cast<int64_t>(_val.count) << ")";
		return _os;
	}

	void DecrementCount(int8_t _amount) {
		if (count - _amount < 0) {
			count = 0;
		} else {
			count -= _amount;
		}
		if (count <= 0) {
			id = Items::Id::INVALID;
			data = 0;
		}
	}

	std::string Str() const {
		std::ostringstream oss;
		oss << *this;
		return oss.str();
	}
};

namespace std {
template <>
struct hash<ItemStack> {
	size_t operator()(const ItemStack& _s) const noexcept {
		size_t h = 0;
		HashCombine(h, _s.id);
		HashCombine(h, _s.count);
		HashCombine(h, _s.data);
		return h;
	}
};
} // namespace std