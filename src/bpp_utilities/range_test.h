/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "logger.h"
#include <climits>
#include <cstdint>
namespace Test {
// Total only works for positive-only values
uint64_t total = 0;
uint64_t count = 0;
int64_t min = INT64_MAX;
int64_t max = INT64_MIN;
bool hasNegatives = false;
void NumberRange(int _value) {
	if (_value < 0)
		hasNegatives = true;
	total += _value;
	count++;
	if (_value < min)
		min = _value;
	if (_value > max)
		max = _value;
	Logger::Info << std::format("{} | min:{} | max:{}, avg:{}, (-)={}\n", _value, min, max,
	                            double(total) / double(count), hasNegatives);
}
} // namespace Test