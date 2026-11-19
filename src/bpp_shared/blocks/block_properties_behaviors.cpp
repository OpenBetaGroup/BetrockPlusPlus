/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "block_properties_behaviors.h"
#include "block_behaviors.h"
#include "block_properties.h"

namespace Blocks {
void RegisterAll() {
	// Default all behavior slots to full-cube before per-block overrides
	RegisterBlockProperties();
	RegisterBlockBehaviors();
}
} // namespace Blocks