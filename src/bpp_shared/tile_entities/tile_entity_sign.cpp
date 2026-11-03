/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "tile_entity_sign.h"

void TileEntitySign::Deserialize(const Tag& _tag) {
	if (_tag.Has("Text1"))
		this->text1 = _tag.Get("Text1").GetString();
	if (_tag.Has("Text2"))
		this->text2 = _tag.Get("Text2").GetString();
	if (_tag.Has("Text3"))
		this->text3 = _tag.Get("Text3").GetString();
	if (_tag.Has("Text4"))
		this->text4 = _tag.Get("Text4").GetString();
}

Tag TileEntitySign::Serialize() {
	auto root = TileEntity::Serialize();

	auto vText1 = Tag{ .type = TAG_STRING, .name = "Text1", .longValue = 0, .stringValue = text1 };
	auto vText2 = Tag{ .type = TAG_STRING, .name = "Text2", .longValue = 0, .stringValue = text2 };
	auto vText3 = Tag{ .type = TAG_STRING, .name = "Text3", .longValue = 0, .stringValue = text3 };
	auto vText4 = Tag{ .type = TAG_STRING, .name = "Text4", .longValue = 0, .stringValue = text4 };

	root.compound["Text1"] = vText1;
	root.compound["Text2"] = vText2;
	root.compound["Text3"] = vText3;
	root.compound["Text4"] = vText4;

	return root;
}