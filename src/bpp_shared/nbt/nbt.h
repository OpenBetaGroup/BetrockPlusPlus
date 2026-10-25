/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once
#include "logger.h"
#include <bit>
#include <cstdint>
#include <cstring>
#include <format>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "helpers/endian_convert.h"
#include "nbt_tag.h"

struct NBTwriter {
	size_t pos;
	NBTwriter() = default;
	NBTwriter(std::vector<uint8_t>& _out, Tag& _root) : pos(0) {
		// root should be a TAG_Compound with whatever name you want (usually "")
		// writeTag handles type byte + name + payload + TAG_END automatically
		_out.resize(WriteTag(_out, _root, false, true));
		WriteTag(_out, _root, false, false);
	}

	size_t WriteTag(std::vector<uint8_t>& _out, const Tag& _tag, const bool _payload = false,
	                const bool _dryRun = false) {
		size_t size = 0;

		if (!_payload) {
			size += sizeof(uint8_t);
			if (!_dryRun)
				_out[pos++] = (uint8_t(_tag.type));
		}
		if (!_payload && _tag.type != TAG_END)
			size += WriteString(_out, _tag.name, _dryRun);

		switch (_tag.type) {
		case TAG_END:
			break;
		case TAG_BYTE:
			size += WriteI8(_out, _tag.byteValue, _dryRun);
			break;
		case TAG_SHORT:
			size += WriteI16(_out, _tag.shortValue, _dryRun);
			break;
		case TAG_INT:
			size += WriteI32(_out, _tag.intValue, _dryRun);
			break;
		case TAG_LONG:
			size += WriteI64(_out, _tag.longValue, _dryRun);
			break;
		case TAG_FLOAT:
			size += WriteF32(_out, _tag.floatValue, _dryRun);
			break;
		case TAG_DOUBLE:
			size += WriteF64(_out, _tag.doubleValue, _dryRun);
			break;
		case TAG_STRING:
			size += WriteString(_out, _tag.stringValue, _dryRun);
			break;

		case TAG_BYTEARRAY: {
			size += WriteI32(_out, int32_t(_tag.byteArray.size()), _dryRun);
			size += _tag.byteArray.size();
			if (_dryRun)
				break;
			// Bytes have no endianness, so a straight copy is correct
			memcpy(_out.data() + pos, _tag.byteArray.data(), _tag.byteArray.size());
			pos += _tag.byteArray.size();
			break;
		}

		case TAG_INTARRAY: {
			size += WriteI32(_out, int32_t(_tag.intArray.size()), _dryRun);
			for (const int32_t v : _tag.intArray)
				size += WriteI32(_out, v, _dryRun);
			break;
		}

		case TAG_LIST: {
			size += WriteI8(_out, int8_t(_tag.listType), _dryRun);
			size += WriteI32(_out, int32_t(_tag.list.size()), _dryRun);
			for (const Tag& element : _tag.list)
				size += WriteTag(_out, element, true, _dryRun);
			break;
		}

		case TAG_COMPOUND: {
			for (const auto& [key, child] : _tag.compound)
				size += WriteTag(_out, child, false, _dryRun);
			// TAG_END terminates the compound
			size += WriteI8(_out, int8_t(TAG_END), _dryRun);
			break;
		}

		default:
			throw std::runtime_error("Unknown tag type: " + std::to_string(_tag.type));
		}

		return size;
	}

	// Write helpers
	// Writes any trivially copyable value in big endian and advances pos.
	template <typename T>
	inline size_t WriteBE(std::vector<uint8_t>& _out, const T _v, const bool _dryRun = false) {
		if (!_dryRun) {
			const T be = ToBigEndian(_v);
			memcpy(_out.data() + pos, &be, sizeof(T));
			pos += sizeof(T);
		}
		return sizeof(T);
	}

	inline size_t WriteI8(std::vector<uint8_t>& _out, const int8_t _v, const bool _dryRun = false) {
		return WriteBE<int8_t>(_out, _v, _dryRun);
	}

	inline size_t WriteI16(std::vector<uint8_t>& _out, const int16_t _v, const bool _dryRun = false) {
		return WriteBE<int16_t>(_out, _v, _dryRun);
	}

	inline size_t WriteI32(std::vector<uint8_t>& _out, const int32_t _v, const bool _dryRun = false) {
		return WriteBE<int32_t>(_out, _v, _dryRun);
	}

	inline size_t WriteI64(std::vector<uint8_t>& _out, const int64_t _v, const bool _dryRun = false) {
		return WriteBE<int64_t>(_out, _v, _dryRun);
	}

	inline size_t WriteF32(std::vector<uint8_t>& _out, const float _v, const bool _dryRun = false) {
		return WriteBE<float>(_out, _v, _dryRun);
	}

	inline size_t WriteF64(std::vector<uint8_t>& _out, const double _v, const bool _dryRun = false) {
		return WriteBE<double>(_out, _v, _dryRun);
	}

	inline size_t WriteString(std::vector<uint8_t>& _out, const std::string& _s, const bool _dryRun = false) {
		if (!_dryRun) {
			WriteI16(_out, int16_t(_s.size()));
			memcpy(_out.data() + pos, _s.data(), _s.size());
			pos += _s.size();
		}
		return sizeof(int16_t) + _s.size();
	}
};

struct NBTParser {
	uint8_t* data;
	size_t length;
	size_t pos;
	Tag root;

	NBTParser() = default;
	NBTParser(uint8_t* _pdata, size_t _plength) : data(_pdata), length(_plength), pos(0) {
		root = ParseTag();
		if (root.type != TAG_COMPOUND)
			throw std::runtime_error("NBT root tag is not a compound!");
	}

	// Parse a tag, either with type and name bytes (parseTag) or just a payload (parsePayload)
	Tag ParsePayload(TagType _ptype, const std::string& _pname = "") {
		Tag tag{ _ptype, _pname, {} };

		switch (_ptype) {
		case TAG_BYTE:
			tag.byteValue = ReadI8();
			break;
		case TAG_SHORT:
			tag.shortValue = ReadI16();
			break;
		case TAG_INT:
			tag.intValue = ReadI32();
			break;
		case TAG_LONG:
			tag.longValue = ReadI64();
			break;
		case TAG_FLOAT:
			tag.floatValue = ReadF32();
			break;
		case TAG_DOUBLE:
			tag.doubleValue = ReadF64();
			break;
		case TAG_STRING:
			tag.stringValue = ReadString();
			break;

		case TAG_BYTEARRAY: {
			int32_t count = ReadI32();
			if (count < 0 || pos + size_t(count) > length) [[unlikely]]
				throw std::runtime_error("NBT: byte array out of bounds");
			tag.byteArray.resize(count);
			std::memcpy(tag.byteArray.data(), data + pos, count);
			pos += count;
			break;
		}

		case TAG_INTARRAY: {
			int32_t count = ReadI32();
			if (count < 0 || pos + size_t(count) * sizeof(int32_t) > length) [[unlikely]]
				throw std::runtime_error("NBT: int array out of bounds");
			tag.intArray.resize(count);
			for (auto& v : tag.intArray)
				v = ReadI32();
			break;
		}

		case TAG_LIST: {
			int8_t innerType = ReadI8();
			int32_t count = ReadI32();

			if (innerType == TAG_END && count > 0)
				throw std::runtime_error("Invalid TAG_List");

			tag.list.reserve(size_t(count));
			for (int i = 0; i < count; i++)
				tag.list.emplace_back(ParsePayload(TagType(innerType)));

			tag.listType = TagType(innerType);
			break;
		}

		case TAG_COMPOUND: {
			while (true) {
				Tag child = ParseTag();
				if (child.type == TAG_END)
					break;
				tag.compound.try_emplace(child.name, std::move(child));
			}
			break;
		}

		default:
			throw std::runtime_error("Unsupported payload type in list");
		}

		return tag;
	}

	// Parse a tag including its type byte and name
	Tag ParseTag() {
		if (pos >= length)
			throw std::runtime_error("Unexpected end of NBT data");

		TagType type = TagType(data[pos++]);
		if (type == TAG_END)
			return Tag{ TAG_END, "", {} }; // no name for TAG_End

		std::string name = ReadString();
		return ParsePayload(type, name);
	}

	// Read helpers
	// Reads any trivially copyable value stored in big endian and advances pos.
	template <typename T>
	inline T ReadBE() {
		if (pos + sizeof(T) > length)
			throw std::runtime_error("NBT: unexpected end");
		T v;
		std::memcpy(&v, data + pos, sizeof(T));
		pos += sizeof(T);
		return FromBigEndian(v);
	}

	inline int8_t ReadI8() { return ReadBE<int8_t>(); }
	inline int16_t ReadI16() { return ReadBE<int16_t>(); }
	inline int32_t ReadI32() { return ReadBE<int32_t>(); }
	inline int64_t ReadI64() { return ReadBE<int64_t>(); }
	inline float ReadF32() { return ReadBE<float>(); }
	inline double ReadF64() { return ReadBE<double>(); }

	inline std::string ReadString() {
		uint16_t len = uint16_t(ReadI16());
		if (pos + len > length)
			throw std::runtime_error(std::format("NBT: string out of bounds ({}+{}/{})", pos, len, length));
		std::string s(reinterpret_cast<const char*>(data) + pos, len);
		pos += len;
		return s;
	}
};