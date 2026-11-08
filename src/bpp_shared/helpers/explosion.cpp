/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, jomner <github.com/jomner>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "entities/entity_mob.h"
#include "gamerules.h"
#include "raycast.h"
#include "world.h"

// for handling explosions
namespace Explosion {

// This function destroys the blocks but does NOT create particle effects / sounds. That is the callers responsibility.
std::unordered_set<Int3> DoExplosion(WorldManager& _world, Entity* _exploder, Vec3 _position, float _size,
                                     bool _doFire) {
	Java::Random rand;
	uint8_t rayGridSize = 16;
	std::unordered_set<Int3> destroyedBlockPositions;

	for (int gridX = 0; gridX < rayGridSize; gridX++) {
		for (int gridY = 0; gridY < rayGridSize; gridY++) {
			for (int gridZ = 0; gridZ < rayGridSize; gridZ++) {
				// Ray trace the outer shell of the explosion
				if (gridX == 0 || gridX == rayGridSize - 1 || gridY == 0 || gridY == rayGridSize - 1 || gridZ == 0 ||
				    gridZ == rayGridSize - 1) {
					double dirX = float(gridX) / (float(rayGridSize) - 1.0F) * 2.0F - 1.0F;
					double dirY = float(gridY) / (float(rayGridSize) - 1.0F) * 2.0F - 1.0F;
					double dirZ = float(gridZ) / (float(rayGridSize) - 1.0F) * 2.0F - 1.0F;
					double dirLength = std::sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);
					dirX /= dirLength;
					dirY /= dirLength;
					dirZ /= dirLength;
					float power = _size * (0.7f + rand.NextFloat() * 0.6f);
					Vec3 rayPos = _position;

					for (float stepSize = 0.3F; power > 0.0F; power -= stepSize * 0.75F) {
						int blockX = MathHelper::FloorDouble(rayPos.x);
						int blockY = MathHelper::FloorDouble(rayPos.y);
						int blockZ = MathHelper::FloorDouble(rayPos.z);
						int blockId = _world.GetBlockId({ blockX, blockY, blockZ });
						if (blockId > BLOCK_AIR) {
							const auto& props = Blocks::blockProperties[blockId];
							float blockResistance = (props.resistance >= 0.0f) ? (props.resistance * 3.0f / 5.0f)
							                                                   : std::max(0.0f, props.hardness);
							power -= (blockResistance + 0.3F) * stepSize;
						}

						if (power > 0.0F) {
							destroyedBlockPositions.insert({ blockX, blockY, blockZ });
						}

						rayPos.x += dirX * double(stepSize);
						rayPos.y += dirY * double(stepSize);
						rayPos.z += dirZ * double(stepSize);
					}
				}
			}
		}
	}

	// Mobs (creepers) leave blocks alone when mob_griefing is off, but still hurt and push entities
	if (dynamic_cast<MobEntity*>(_exploder) && !Gamerule::Get<bool>(Gamerule::Id::MobGriefing))
		destroyedBlockPositions.clear();

	// Damage entities
	auto expandedExplosionSize = _size * 2.0f;
	double minX = MathHelper::FloorDouble(_position.x - double(expandedExplosionSize) - 1.0);
	double maxX = MathHelper::FloorDouble(_position.x + double(expandedExplosionSize) + 1.0);
	double minY = MathHelper::FloorDouble(_position.y - double(expandedExplosionSize) - 1.0);
	double maxY = MathHelper::FloorDouble(_position.y + double(expandedExplosionSize) + 1.0);
	double minZ = MathHelper::FloorDouble(_position.z - double(expandedExplosionSize) - 1.0);
	double maxZ = MathHelper::FloorDouble(_position.z + double(expandedExplosionSize) + 1.0);
	auto affectedEntities = _world.entityManager.GetEntitiesWithinAabbExcluding({ minX, minY, minZ, maxX, maxY, maxZ },
	                                                                            _exploder ? _exploder->id
	                                                                                      : EntityId(-1));
	auto getBlockDensity = [&_world](Vec3 _explosionCenter, const AABB& _entityBox) -> double {
		double stepX = 1.0 / ((_entityBox.maxX - _entityBox.minX) * 2.0 + 1.0);
		double stepY = 1.0 / ((_entityBox.maxY - _entityBox.minY) * 2.0 + 1.0);
		double stepZ = 1.0 / ((_entityBox.maxZ - _entityBox.minZ) * 2.0 + 1.0);
		int clearSamples = 0;
		int totalSamples = 0;

		for (float fracX = 0.0f; fracX <= 1.0f; fracX = float(double(fracX) + stepX)) {
			for (float fracY = 0.0f; fracY <= 1.0f; fracY = float(double(fracY) + stepY)) {
				for (float fracZ = 0.0f; fracZ <= 1.0f; fracZ = float(double(fracZ) + stepZ)) {
					Vec3 samplePos = { _entityBox.minX + (_entityBox.maxX - _entityBox.minX) * double(fracX),
						               _entityBox.minY + (_entityBox.maxY - _entityBox.minY) * double(fracY),
						               _entityBox.minZ + (_entityBox.maxZ - _entityBox.minZ) * double(fracZ) };

					RayCastResult hit = Raycast::Raycast(_world, samplePos, _explosionCenter,
					                                     RayCastMode::IGNORE_FLUIDS);
					if (!hit.hit) {
						++clearSamples;
					}
					++totalSamples;
				}
			}
		}

		return double(clearSamples) / double(totalSamples);
	};

	// Deal damage based on distance / covering
	for (auto& entity : affectedEntities) {
		double distanceFraction = entity->position.Distance(_position) / expandedExplosionSize;
		if (distanceFraction <= 1.0) {
			double dx = entity->position.x - _position.x;
			double dy = entity->position.y - _position.y;
			double dz = entity->position.z - _position.z;
			double offsetLength = std::sqrt(dx * dx + dy * dy + dz * dz);
			dx /= offsetLength;
			dy /= offsetLength;
			dz /= offsetLength;
			double blockDensity = getBlockDensity(_position, entity->collider);
			double impactStrength = (1.0 - distanceFraction) * blockDensity;
			entity->AttackEntityFrom(_exploder,
			                         int((impactStrength * impactStrength + impactStrength) / 2.0 * 8.0 *
			                                 double(expandedExplosionSize) +
			                             1.0),
			                         DamageType::Explosion);
			entity->velocity.x += dx * impactStrength;
			entity->velocity.y += dy * impactStrength;
			entity->velocity.z += dz * impactStrength;
		}
	}

	// Apply fire / drop items
	for (auto& pos : destroyedBlockPositions) {
		int blockIdAt = _world.GetBlockId(pos);
		if (_doFire) {
			int belowBlock = _world.GetBlockId({ pos.x, pos.y - 1, pos.z });
			if (blockIdAt == BLOCK_AIR && Blocks::blockProperties[belowBlock].isOpaqueCube && rand.NextInt(3) == 0) {
				_world.SetBlock(pos, BLOCK_FIRE);
			}
		}
		if (auto func = Blocks::blockBehaviors[blockIdAt].onBlockDestroyedByExplosion) {
			func(_world, pos);
		}
	}

	return destroyedBlockPositions;
}

} // namespace Explosion