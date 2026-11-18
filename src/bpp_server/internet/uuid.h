/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#if defined(ONLINE_MODE_AUTHENTICATION) || defined(BETACRAFT_HEARTBEAT)
#pragma once
#include <optional>
#include <string>

namespace Uuid {
    std::optional<std::string> UsernameToUuid(const std::string& _username);
    std::optional<std::string> UuidToUsername(const std::string& _uuid);
}
#endif