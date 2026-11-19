/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 * 
*/

#pragma once
#include <cstdint>
#include <string>


constexpr int SLOT_EMPTY = -1; // sentinel for "no slot found"

const std::string IdToLabel(const int16_t _id);