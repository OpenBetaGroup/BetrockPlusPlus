/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once
#include <cstdint>
#include <cstring>
#include <format>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

enum TagType : uint8_t {
	TAG_END,
	TAG_BYTE,
	TAG_SHORT,
	TAG_INT,
	TAG_LONG,
	TAG_FLOAT,
	TAG_DOUBLE,
	TAG_BYTEARRAY,
	TAG_STRING,
	TAG_LIST,
	TAG_COMPOUND,
	TAG_INTARRAY
};

struct Tag {
	TagType type = TAG_END;
	std::string name;

	// Leaf values
	// do this as an anonymous union, since they can share memory
	union {
		int8_t byteValue;
		int16_t shortValue;
		int32_t intValue;
		// this is enough to init all of these
		int64_t longValue = 0;
		float floatValue;
		// all bits as 0 is also 0 in double!
		double doubleValue;
	};
	std::vector<int8_t> byteArray = {};
	std::vector<int32_t> intArray = {};
	std::string stringValue = "";

	// Container values
	TagType listType = TAG_END; // element type for TAG_LIST
	std::vector<Tag> list = {};
	std::unordered_map<std::string, Tag> compound = {};

	// Typed setter
	template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
	void Set(T _value) {
		switch (type) {
		case TAG_BYTE:
			byteValue = _value;
			break;
		case TAG_SHORT:
			shortValue = _value;
			break;
		case TAG_INT:
			intValue = _value;
			break;
		case TAG_LONG:
			longValue = _value;
			break;
		case TAG_FLOAT:
			floatValue = _value;
			break;
		case TAG_DOUBLE:
			doubleValue = _value;
			break;
		default:
			Logger::Warning << "Tried to use numeric setter on non-numeric NBT type " << type << "!\n";
			break;
		}
	}

	void Set(const std::string& _value) {
		if (type == TAG_STRING)
			stringValue = _value;
		else
			Logger::Warning << "Tried to use string setter on non-string NBT type " << type << "!\n";
	}

	// Typed getters; throw if wrong type
	int8_t GetByte() const {
		Expect(TAG_BYTE);
		return byteValue;
	}
	int16_t GetShort() const {
		Expect(TAG_SHORT);
		return shortValue;
	}
	int32_t GetInt() const {
		Expect(TAG_INT);
		return intValue;
	}
	int64_t GetLong() const {
		Expect(TAG_LONG);
		return longValue;
	}
	float GetFloat() const {
		Expect(TAG_FLOAT);
		return floatValue;
	}
	double GetDouble() const {
		Expect(TAG_DOUBLE);
		return doubleValue;
	}
	const std::vector<int8_t>& GetByteArray() const {
		Expect(TAG_BYTEARRAY);
		return byteArray;
	}
	const std::vector<int32_t>& GetIntArray() const {
		Expect(TAG_INTARRAY);
		return intArray;
	}
	const std::string& GetString() const {
		Expect(TAG_STRING);
		return stringValue;
	}
	const std::vector<Tag>& GetList() const {
		Expect(TAG_LIST);
		return list;
	}
	const std::unordered_map<std::string, Tag>& GetCompound() const {
		Expect(TAG_COMPOUND);
		return compound;
	}

	// Compound lookup helpers
	bool Has(const std::string& _key) const {
		return compound.contains(_key);
	}

	const Tag& Get(const std::string& _key) const {
		auto it = compound.find(_key);
		if (it == compound.end()) {
			Logger::Error << "Tried to access a NBT tag that doesn't exist! (" << _key << ")\n";
			throw std::runtime_error("NBT tag doesn't exist!");
		}
		return it->second;
	}

	bool Matches(const Tag& _pattern) const {
		if (type != _pattern.type) return false;

		switch (type) {
		case TAG_END: return true;
		case TAG_BYTE: return byteValue == _pattern.byteValue;
		case TAG_SHORT: return shortValue == _pattern.shortValue;
		case TAG_INT: return intValue == _pattern.intValue;
		case TAG_LONG: return longValue == _pattern.longValue;
		case TAG_FLOAT: return floatValue == _pattern.floatValue;
		case TAG_DOUBLE: return doubleValue == _pattern.doubleValue;
		case TAG_STRING: return stringValue == _pattern.stringValue;
		case TAG_BYTEARRAY: return byteArray == _pattern.byteArray;
		case TAG_INTARRAY: return intArray == _pattern.intArray;
		case TAG_COMPOUND:
			for (const auto& [key, pattern] : _pattern.compound) {
				auto found = compound.find(key);
				if (found == compound.end() || !found->second.Matches(pattern)) return false;
			}
			return true;
		case TAG_LIST:
			if (_pattern.list.empty()) return list.empty();
			for (const auto& pattern : _pattern.list) {
				bool found = false;

				for (const auto& actual : list) {
					if (actual.Matches(pattern)) { found = true; break; }
				}
				if (!found) return false;
			}
			return true;
		}
		return false;
	}

private:
	void Expect(TagType _t) const {
		if (type != _t) {
			Logger::Error << "Unexpected NBT type in getter! '" << _t << "'. Expected: '" << type << "'\n";
			throw std::runtime_error("Unexpected NBT type!");
		}
	}
};