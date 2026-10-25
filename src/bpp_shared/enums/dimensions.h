/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 * 
*/

#pragma once
#include <cstdint>

// Used as the identifier for the current dimension
// The End would become Dimenion id 1

enum class Dimension : int8_t {
	// The nether, also known as hell or the slip
	Nether = -1,
	// The overworld, default dimension
	Overworld = 0,
	// The sky dimension, unused, but still available
	Sky = 1
};