/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../command.h"
#include "../command_manager.h"
#include "../command_registry.h"
#include "server.h"
#include <algorithm>
#include <cstdint>
#include <format>

namespace {

// Weather only exists in the overworld
WorldManager& Overworld(CommandContext& _ctx) {
	return _ctx.server->gameRuntime.world;
}

std::string FormatTicks(int32_t _ticks) {
	int32_t totalSeconds = std::max<int32_t>(0, _ticks) / 20;
	return std::format("{} ticks ({}m {}s)", _ticks, totalSeconds / 60, totalSeconds % 60);
}

const char* WeatherName(Weather _weather) {
	switch (_weather) {
	case Weather::Thundering:
		return "Thunder";
	case Weather::Raining:
		return "Rain";
	default:
		return "Clear";
	}
}

// Sets the target weather
void ApplyWeather(WorldManager& _world, Weather _target) {
	auto& weather = _world.weatherSystem;
	auto& rand = _world.rand;

	const bool rain = _target != Weather::Clear;
	const bool thunder = _target == Weather::Thundering;

	weather.isRaining = rain;
	weather.isThundering = thunder;
	weather.rainTimer = rain ? rand.NextInt(12000) + 12000 : rand.NextInt(168000) + 12000;
	weather.thunderTimer = thunder ? rand.NextInt(12000) + 3600 : rand.NextInt(168000) + 12000;

	weather.rainStrength = weather.prevRainStrength = rain ? 1.0f : 0.0f;
	weather.thunderStrength = weather.prevThunderStrength = thunder ? 1.0f : 0.0f;
}

std::string GetWeather(const strategos::CmdNode&, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	const auto& weather = Overworld(ctx).weatherSystem;

	SendFeedback(ctx, std::format("§eWeather: {}", WeatherName(weather.GetWeatherState())));
	SendFeedback(ctx, std::format("§eRain {} in {}", weather.isRaining ? "stops" : "starts",
	                                   FormatTicks(weather.rainTimer)));
	SendFeedback(ctx, std::format("§eThunder {} in {}", weather.isThundering ? "stops" : "starts",
	                                   FormatTicks(weather.thunderTimer)));
	return "";
}

std::string SetWeather(void* _userData, Weather _target) {
	auto& ctx = CmdCtx(_userData);
	auto& world = Overworld(ctx);
	ApplyWeather(world, _target);

	const auto& weather = world.weatherSystem;
	int32_t duration = _target == Weather::Thundering ? std::min(weather.rainTimer, weather.thunderTimer)
	                                                  : weather.rainTimer;
	SendFeedback(ctx, std::format("§eSet weather to {} for {}", WeatherName(_target), FormatTicks(duration)));
	return "";
}

std::string SetClear(const strategos::CmdNode&, void* _userData) {
	return SetWeather(_userData, Weather::Clear);
}

std::string SetRain(const strategos::CmdNode&, void* _userData) {
	return SetWeather(_userData, Weather::Raining);
}

std::string SetThunder(const strategos::CmdNode&, void* _userData) {
	return SetWeather(_userData, Weather::Thundering);
}

} // namespace

void RegisterWeather(strategos::BrigadierContext& _dispatcher) {
	_dispatcher.add_command(strategos::Node::literal("weather")
	                            .describe("Gets or sets the current weather")
	                            .op()
	                            .executes(GetWeather)
	                            .then(strategos::Node::literal("set")
	                                      .then(strategos::Node::literal("clear").executes(SetClear))
	                                      .then(strategos::Node::literal("rain").executes(SetRain))
	                                      .then(strategos::Node::literal("thunder").executes(SetThunder))));
}
