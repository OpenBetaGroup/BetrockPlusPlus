/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "tile_entity_jukebox.h"

void TileEntityJukebox::Deserialize(const Tag& _tag) {
	// no-op
	return;
}

Tag TileEntityJukebox::Serialize() {
	auto root = TileEntity::Serialize();
	root.compound["Record"] = Tag{ .type = TAG_INT, .name = "Record", .intValue = static_cast<int32_t>(recordItemId) };
	return root;
}