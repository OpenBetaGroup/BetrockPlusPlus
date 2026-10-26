/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once
#include "logger.h"
#include "nbt_tag.h"
#include <cctype>

struct NBTTextifier {
	enum Mode : uint8_t {
		FULL,
		REDUCED
	};

	// Converts an NBT compound to a string
	std::string Parse(Tag& _tag, Mode /*_mode*/) {
		std::string parsed;

		if (_tag.type != TAG_COMPOUND)
			Throw("Root tag must be a compound!");

		ParseTag(_tag, parsed, _tag.name, /*inList=*/false, /*inCompound=*/true);

		return parsed;
	}

	// Parse a singular tag into text
	void ParseTag(Tag& _tag, std::string& _string, std::string _name, bool inList = false, bool inCompound = false,
	              bool writeName = true, bool forceComma = false) {
		// Write the name of our tag
		if (_tag.type != TAG_COMPOUND) {
			if (writeName) {
				_string.append("\"");
				_string.append(EscapeString(_name));
				_string.append("\": ");
			}
		} else if (_name != "") {
			_string.append("\"");
			_string.append(EscapeString(_name));
			_string.append("\": ");
		}

		switch (_tag.type) {
		case TagType::TAG_COMPOUND: {
			_string.append("{");
			if (!inList)
				_string.append("\n");
			auto length = _tag.compound.size();
			size_t index = 0;
			for (auto& tag : _tag.compound) {
				ParseTag(tag.second, _string, tag.first, index < length - 1 ? false : true, true, true);
				index++;
			}
			if (!inList)
				_string.append("\n");
			_string.append("}");
			if (!inList)
				_string.append(", \n");
			else if (forceComma)
				_string.append(", ");
			return;
		}
		case TagType::TAG_LIST: {
			_string.append("[");
			auto length = _tag.list.size();
			size_t index = 0;
			for (auto& tag : _tag.list) {
				ParseTag(tag, _string, tag.name, true, inCompound, false);
				if (index < length - 1)
					_string.append(", ");
				index++;
			}
			_string.append("]");
			if (!inList)
				_string.append(", \n");
			else if (forceComma)
				_string.append(", ");
			return;
		}
		case TagType::TAG_BYTEARRAY: {
			_string.append("[b; ");
			auto length = _tag.byteArray.size();
			size_t index = 0;
			for (auto& byte : _tag.byteArray) {
				_string.append(std::to_string(byte) + "b");
				if (index < length - 1)
					_string.append(", ");
				index++;
			}
			_string.append("]");
			if (!inList)
				_string.append(", \n");
			else if (forceComma)
				_string.append(", ");
			return;
		}
		case TagType::TAG_INTARRAY: {
			_string.append("[i; ");
			auto length = _tag.intArray.size();
			size_t index = 0;
			for (auto& integer : _tag.intArray) {
				_string.append(std::to_string(integer));
				if (index < length - 1)
					_string.append(", ");
				index++;
			}
			_string.append("]");
			if (!inList)
				_string.append(", \n");
			else if (forceComma)
				_string.append(", ");
			return;
		}
		case TagType::TAG_BYTE: {
			_string.append(std::to_string(_tag.GetByte()));
			_string.append("b");
			if (!inList)
				_string.append(", \n");
			else if (forceComma)
				_string.append(", ");
			return;
		}
		case TagType::TAG_SHORT: {
			_string.append(std::to_string(_tag.GetShort()));
			_string.append("s");
			if (!inList)
				_string.append(", \n");
			else if (forceComma)
				_string.append(", ");
			return;
		}
		case TagType::TAG_INT: {
			_string.append(std::to_string(_tag.GetInt()));
			if (!inList)
				_string.append(", \n");
			else if (forceComma)
				_string.append(", ");
			return;
		}
		case TagType::TAG_LONG: {
			_string.append(std::to_string(_tag.GetLong()));
			_string.append("L");
			if (!inList)
				_string.append(", \n");
			else if (forceComma)
				_string.append(", ");
			return;
		}
		case TagType::TAG_FLOAT: {
			_string.append(std::to_string(_tag.GetFloat()));
			_string.append("f");
			if (!inList)
				_string.append(", \n");
			else if (forceComma)
				_string.append(", ");
			return;
		}
		case TagType::TAG_DOUBLE: {
			_string.append(std::to_string(_tag.GetDouble()));
			_string.append("d");
			if (!inList)
				_string.append(", \n");
			else if (forceComma)
				_string.append(", ");
			return;
		}
		case TagType::TAG_STRING: {
			_string.append("\"");
			_string.append(EscapeString(_tag.GetString()));
			_string.append("\"");
			if (!inList)
				_string.append(", \n");
			else if (forceComma)
				_string.append(", ");
			return;
		}
		case TagType::TAG_END: {
			return;
		}
		}
	}

private:
	// Escapes characters that would break a quoted literal
	static std::string EscapeString(const std::string& _in) {
		std::string out;
		out.reserve(_in.size());
		for (const char c : _in) {
			switch (c) {
			case '\\':
				out += "\\\\";
				break;
			case '"':
				out += "\\\"";
				break;
			case '\n':
				out += "\\n";
				break;
			case '\r':
				out += "\\r";
				break;
			case '\t':
				out += "\\t";
				break;
			default:
				out += c;
				break;
			}
		}
		return out;
	}

	void Throw(std::string _reason) const {
		Logger::Error << _reason << "\n";
		throw std::runtime_error(_reason);
	}

	void Warn(std::string _reason) const {
		Logger::Warning << _reason << "\n";
	}
};

struct TextNBTifier {
	// Converts a string to an NBT compound
	std::string nbtString;
	size_t stepper = 0;

	Tag Parse(std::string _string) {
		nbtString = _string;
		stepper = 0;
		std::vector<Token> tokens;
		tokens.clear();
		Lex(&tokens, stepper);

		stepper = 0;
		return Link(&tokens);
	}

private:
	enum TokenIdentifier : uint8_t {
		COMPOUND_BEGIN, // {
		COMPOUND_END,   // }
		LIST_BEGIN,     // [
		LIST_END,       // ]
		COLON,          // :
		COMMA,          // ,
		ARRAY_PREFIX,   // B; I; L;
		STRING_LITERAL,
		NUMBER_LITERAL, // e.g., "1b", "300s", "1.5f", "42"
		INVALID
	};

	struct Token {
		TokenIdentifier identifier = INVALID;
		std::string tokenText = "";
		TagType type = TAG_END;
	};

	// Linker
	Tag Link(std::vector<Token>* _tokens);
	Tag LinkTag(std::vector<Token>* _tokens, size_t& _stepper, std::string& _tagName);
	Tag LinkList(std::vector<Token>* _tokens, size_t& _stepper, std::string _name);
	Tag LinkCompound(std::vector<Token>* _tokens, size_t& _stepper, std::string _name);
	void SetNumberFromText(Tag& _tag, std::string _text);

	// Lexer
	void Lex(std::vector<Token>* _tokens, size_t& _stepper);
	void LexToken(std::vector<Token>* _tokens, size_t& _stepper);
	void LexLiteral(std::vector<Token>* _tokens, size_t& _stepper);
	void LexList(std::vector<Token>* _tokens, size_t& _stepper);
	bool IsArrayIdentifier(std::vector<Token>* _tokens, size_t& _stepper);
	void LexCompound(std::vector<Token>* _tokens, size_t& _stepper);

	bool IsBreakChar(const char _char) {
		return _char == ':' || _char == ',';
	}

	bool IsDigit(const char _char) {
		return _char == '0' || _char == '1' || _char == '2' || _char == '3' || _char == '4' || _char == '5' ||
		       _char == '6' || _char == '7' || _char == '8' || _char == '9' || _char == '.';
	}

	// NOTE: These don't have safety! Be careful!
	void Throw(std::string _reason) const {
		Logger::Error << _reason << "\n";
		throw std::runtime_error(_reason);
	}

	void Warn(std::string _reason) const {
		Logger::Warning << _reason << "\n";
	}

	bool IndexInBounds(size_t _index) const {
		return _index < nbtString.length();
	}

	char Peek() {
		if (!IndexInBounds(stepper))
			return '\0';
		return nbtString[stepper];
	}

	char PeekNext() {
		if (!IndexInBounds(stepper + 1))
			return '\0';
		return nbtString[stepper + 1];
	}

	// Seeks to the index in the string and sets the stepper
	char Seek(int64_t _index) {
		stepper = _index;
		return Peek();
	}

	// Advances the stepper
	void Advance() {
		stepper++;
	}

	// Skips until a non-whitespace character is provided
	// Returns true if it skipped characters
	bool SkipWhitespace() {
		size_t savedStep = stepper;
		while (std::isspace(nbtString[stepper])) {
			stepper++;
		}
		return savedStep != stepper;
	}
};