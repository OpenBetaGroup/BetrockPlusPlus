/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 * 
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once
#include "numeric_structs.h"
#include <climits>

// For checking if anything is in a valid, or expected range

inline bool IsBlockInWorldBounds(const Int2 _pos) {
    return (_pos.x <= INT32_MAX) &&
        (_pos.z <= INT32_MAX) &&
        (_pos.x >= INT32_MIN) &&
        (_pos.z >= INT32_MIN);
}
inline bool IsBlockInWorldBounds(const Int3 _pos) {
    return (_pos.x <= INT32_MAX) &&
        (_pos.y <= (CHUNK_HEIGHT - 1)) &&
        (_pos.z <= INT32_MAX) &&
        (_pos.x >= INT32_MIN) &&
        (_pos.x >= 0) &&
        (_pos.y >= INT32_MIN);
}
inline bool IsChunkInWorldBounds(const Int2 _pos) {
    return (_pos.x <= INT32_MAX >> 4) &&
        (_pos.y <= INT32_MAX >> 4) &&
        (_pos.x >= INT32_MIN >> 4) &&
        (_pos.y >= INT32_MIN >> 4);
}