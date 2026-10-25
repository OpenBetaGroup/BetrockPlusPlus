/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#include <algorithm>
#include <cstdint>
#include <functional>
#include <vector>

template <typename... Args>
class EventEmitter {
	using Callback = std::function<void(Args...)>;

public:
	using SubscriberId = uint64_t;

	SubscriberId Subscribe(Callback _callback) {
		SubscriberId id = nextId++;
		subscribers.push_back({ id, std::move(_callback) });
		return id;
	}

	void Unsubscribe(SubscriberId _id) {
		subscribers.erase(std::remove_if(subscribers.begin(), subscribers.end(), [_id](auto& s) { return s.id == _id; }),
		                  subscribers.end());
	}

	void Call(Args... _args) {
		for (auto& s : subscribers) {
			s.callback(std::forward<Args>(_args)...);
		}
	}

	void Clear() {
		subscribers.clear();
	}

	size_t Size() const {
		return subscribers.size();
	}

	bool Empty() const {
		return subscribers.empty();
	}

private:
	struct Subscriber {
		SubscriberId id;
		Callback callback;
	};

	SubscriberId nextId = 0;
	std::vector<Subscriber> subscribers;
};