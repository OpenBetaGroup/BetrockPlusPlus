/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 * 
*/

#pragma once
#include "../enums/blocks.h"
#include "../enums/network/packet_data.h"
#include "direction.h"
#include <cstdint>

/**
 * @brief Turn a metadata value into an appropriate direction for the block
 * 
 * @param _type Block type
 * @param _meta Metadata value
 * @return Cardinal::Value 
 */
Cardinal::Value GetDirectionFromMeta(const BlockType _type, const uint8_t _meta);
uint8_t GetMetaFromDirection(const BlockType _type, const Cardinal::Value _dir);
Cardinal::Value FaceDirectionToDirection(const PacketData::FaceDirection _face);
PacketData::FaceDirection DirectionToFaceDirection(const Cardinal::Value _dir);
Cardinal::Value FromPaintingDirectionToDirection(const PacketData::PaintingDirection _value);
PacketData::PaintingDirection FromDirectionToPaintingDirection(const Cardinal::Value _dir);