/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#include "weather_system.h"
#include "gamerules.h"
#include <algorithm>

void WeatherSystem::Init(int32_t _rainTimer, int32_t _thunderTimer, bool _isRaining, bool _isThundering) {
	rainTimer = _rainTimer;
	thunderTimer = _thunderTimer;
	isRaining = _isRaining;
	isThundering = _isThundering;

	rainStrength = prevRainStrength = isRaining ? 1.0f : 0.0f;
	thunderStrength = prevThunderStrength = (isRaining && isThundering) ? 1.0f : 0.0f;
}

void WeatherSystem::Tick(Java::Random& _rand) {
	if (!Gamerule::Get<bool>(Gamerule::Id::AdvanceWeather))
		return;
	if (thunderTimer <= 0) {
		thunderTimer = isThundering ? _rand.NextInt(12000) + 3600 : _rand.NextInt(168000) + 12000;
	} else if (--thunderTimer <= 0) {
		isThundering = !isThundering;
	}

	if (rainTimer <= 0) {
		rainTimer = isRaining ? _rand.NextInt(12000) + 12000 : _rand.NextInt(168000) + 12000;
	} else if (--rainTimer <= 0) {
		isRaining = !isRaining;
	}

	prevRainStrength = rainStrength;
	rainStrength = std::clamp(rainStrength + (isRaining ? 0.01f : -0.01f), 0.0f, 1.0f);
	prevThunderStrength = thunderStrength;
	thunderStrength = std::clamp(thunderStrength + (isThundering ? 0.01f : -0.01f), 0.0f, 1.0f);
}