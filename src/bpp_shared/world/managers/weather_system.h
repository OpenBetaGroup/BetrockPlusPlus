/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once
#include "../enums/weather.h"
#include "../helpers/java/java_random.h"
#include <cstdint>

// Based on info from https://minecraft.wiki/w/Weather#Java_Edition_mechanics

struct WeatherSystem {
	int32_t rainTimer = 0;
	int32_t thunderTimer = 0;
	bool isRaining : 1 = false;
	bool isThundering : 1 = false;
	void Init(int32_t _rainTimer = 0, int32_t _thunderTimer = 0, bool _isRaining = false, bool _isThundering = false);
	void Tick(Java::Random& _rand);

	float rainStrength = 0.0f, prevRainStrength = 0.0f;
	float thunderStrength = 0.0f, prevThunderStrength = 0.0f;

	bool IsRaining() const {
		float weightedStrength = prevRainStrength + (rainStrength - prevRainStrength);
		return weightedStrength > 0.2f;
	}

	bool IsThundering() const {
		float weightedStrengthRain = prevRainStrength + (rainStrength - prevRainStrength);
		float weightedStrength = prevThunderStrength + (thunderStrength - prevThunderStrength);

		return weightedStrength * weightedStrengthRain > 0.9f;
	}

	void SetRaining(int _timer) {
		isRaining = true;
		rainStrength = 1.0f;
		rainTimer = _timer;
	}

	void SetThundering(int _timer) {
		isRaining = true;
		isThundering = true;
		rainStrength = 1.0f;
		thunderStrength = 1.0f;
		thunderTimer = _timer;
	}

	void ClearWeather() {
		rainTimer = 0;
		isRaining = false;
		thunderTimer = 0;
		isThundering = false;
	}

	Weather GetWeatherState() const {
		if (IsThundering())
			return Weather::Thundering;
		if (IsRaining())
			return Weather::Raining;
		return Weather::Clear;
	}
};