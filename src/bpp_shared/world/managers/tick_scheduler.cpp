/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#include "tick_scheduler.h"
#include "blocks/block_behaviors.h"
#include "world.h"
#include <algorithm>

void TickScheduler::Tick() {
	if (!this->world)
		return;

	currentTick++;

	size_t budget = std::min(scheduledTicks.size(), MAX_TICKS_PER_TICK);
	for (size_t i = 0; i < budget; i++) {
		if (scheduledTicks.top().tickDue > currentTick)
			break;

		ScheduledTick entry = scheduledTicks.top();
		scheduledTicks.pop();
		pending.erase({ entry.pos, entry.expectedBlock }); // Removed before updateTick

		// Only fire if the block is still the one that was scheduled
		if (entry.expectedBlock != BLOCK_AIR && world->GetBlockId(entry.pos) == entry.expectedBlock) {
			if (auto fn = Blocks::blockBehaviors[entry.expectedBlock].onTick)
				fn(*world, entry.pos, world->GetMetadata(entry.pos), world->rand);
		}
	}
}