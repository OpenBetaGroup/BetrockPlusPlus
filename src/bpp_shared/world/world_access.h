/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once
#include "blocks.h"
#include "blocks/block_properties.h"
#include "blocks/materials.h"
#include "numeric_structs.h"
#include <cstdint>

class WorldAccess {
public:
	WorldAccess() = default;
	virtual ~WorldAccess() = default;
	// Convert a world-space position to a region-local chunk offset (-1..1, -1..1)
	virtual bool CanBlockSeeSky(const Int3 _pos) = 0;
	virtual int FindTopSolidBlock(const int _wx, const int _wz) = 0;
	virtual int GetHeightValue(const int _wx, const int _wz) = 0;
	virtual double GetTemperatureAt(const int _wx, const int _wz) = 0;
	virtual double GetHumidityAt(const int _wx, const int _wz) = 0;
	virtual BlockType GetBlockId(const Int3 _wpos) = 0;
	virtual void SetBlock(const Int3 _wpos, const BlockType _type, const uint8_t _meta = 0,
	                      const bool _keepTileEntity = false, const bool _updateNeighbors = true) = 0;
	virtual uint8_t GetSkyLight(const Int3 _wpos) = 0;
	// Combined block light value (max of sky light and emitted block light),
	virtual int GetBlockLightRaw(const Int3 _wpos) = 0;

	Material GetMaterial(const Int3 _wpos) {
		return Blocks::blockProperties[GetBlockId(_wpos)].material;
	}
};
