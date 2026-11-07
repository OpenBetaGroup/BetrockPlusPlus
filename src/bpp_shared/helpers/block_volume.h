/*
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once

#include <cassert>
#include "numeric_structs.h"

template <typename F>
void ForEachBlockInVolume(Int3 _center, int _ext, F&& _fn) {
	assert(_ext >= 0);

	for (int dx = -_ext; dx <= _ext; ++dx) {
		for (int dy = -_ext; dy <= _ext; ++dy) {
			for (int dz = -_ext; dz <= _ext; ++dz) {
				_fn(Int3{ _center.x + dx, _center.y + dy, _center.z + dz });
			}
		}
	}
}
