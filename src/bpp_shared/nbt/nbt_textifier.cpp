/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#include "nbt_textifier.h"

// Sets a tag's appropriate value from a string input
void TextNBTifier::SetNumberFromText(Tag& _tag, std::string _text) {
	// Strip a type suffix if one is still there
	if (!_text.empty() && std::isalpha(static_cast<unsigned char>(_text.back())))
		_text.pop_back();

	try {
		switch (_tag.type) {
		case TAG_BYTE:
		case TAG_SHORT:
		case TAG_INT: {
			const long long v = std::stoll(_text);
			const auto [lo, hi] = _tag.type == TAG_BYTE    ? std::pair<long long, long long>{ INT8_MIN, INT8_MAX }
			                      : _tag.type == TAG_SHORT ? std::pair<long long, long long>{ INT16_MIN, INT16_MAX }
			                                               : std::pair<long long, long long>{ INT32_MIN, INT32_MAX };
			if (v < lo || v > hi)
				Throw("Numeric literal out of range: " + _text);
			_tag.Set(v); // numeric template narrows into the right union member
			break;
		}
		case TAG_LONG:
			_tag.Set(std::stoll(_text));
			break;
		case TAG_FLOAT:
			_tag.Set(std::stof(_text));
			break;
		case TAG_DOUBLE:
			_tag.Set(std::stod(_text));
			break;
		default:
			Throw("Tried to parse a number into a non-numeric tag!");
		}
	} catch (const std::invalid_argument&) {
		Throw("Malformed numeric literal: " + _text);
	} catch (const std::out_of_range&) {
		Throw("Numeric literal out of range: " + _text);
	}
}

// Link a series of tokens into an NBT structure
Tag TextNBTifier::Link(std::vector<Token>* _tokens) {
	if (_tokens->at(0).type != TAG_COMPOUND)
		Throw("Invalid start token while linking!\n");

	// Create the root compound
	std::string tagName = "";

	return LinkTag(_tokens, stepper, tagName);
}

// Links a list of tokens into a tag
Tag TextNBTifier::LinkTag(std::vector<Token>* _tokens, size_t& _stepper, std::string& _tagName) {
	size_t prevIndex = _stepper > 0 ? _stepper - 1 : 0;
	size_t nextIndex = _stepper + 1 > _tokens->size() - 1 ? _tokens->size() - 1 : _stepper + 1;
	auto& token = _tokens->at(_stepper);
	auto& previousToken = _tokens->at(prevIndex);
	auto& nextToken = _tokens->at(nextIndex);
	switch (token.identifier) {
	case COMPOUND_BEGIN: {
		auto Tag = LinkCompound(_tokens, _stepper, _tagName);
		_tagName = "";
		return Tag;
	}
	case LIST_BEGIN: {
		auto Tag = LinkList(_tokens, _stepper, _tagName);
		_tagName = "";
		return Tag;
	}
	case COLON:
	case COMMA:
	case LIST_END:
	case COMPOUND_END:
	case ARRAY_PREFIX:
		return {};
	case STRING_LITERAL: {
		if (nextToken.identifier == COLON) {
			_tagName = token.tokenText;
			return {};
		}

		const bool isCompoundValue = previousToken.identifier == COLON;
		const bool isListElement = previousToken.identifier == COMMA || previousToken.identifier == LIST_BEGIN;
		if (!isCompoundValue && !isListElement)
			Throw("Invalid use of string literal '" + token.tokenText + "' at token " + std::to_string(_stepper) + "!");

		Tag stringTag;
		stringTag.type = TAG_STRING;
		stringTag.name = isCompoundValue ? _tagName : "";
		stringTag.stringValue = token.tokenText;
		_tagName = "";
		return stringTag;
	}
	case NUMBER_LITERAL: {
		if (nextToken.identifier == COLON) {
			_tagName = token.tokenText;
			return {};
		}

		const bool isCompoundValue = previousToken.identifier == COLON;
		const bool isListElement = previousToken.identifier == COMMA || previousToken.identifier == LIST_BEGIN ||
		                           previousToken.identifier == ARRAY_PREFIX;
		if (!isCompoundValue && !isListElement)
			Throw("Invalid use of numeric literal '" + token.tokenText + "' at token " + std::to_string(_stepper) + "!");

		Tag numberTag;
		numberTag.type = token.type;
		numberTag.name = isCompoundValue ? _tagName : "";
		SetNumberFromText(numberTag, token.tokenText);
		_tagName = "";
		return numberTag;
	}
	default:
		Throw("Invalid token while linking NBT structure!");
		return {};
	}
}

// Links together tag lists and specialty int and byte lists
Tag TextNBTifier::LinkList(std::vector<Token>* _tokens, size_t& _stepper, std::string _name) {
	auto& nextToken = _tokens->at(_stepper + 1);
	bool specialList = nextToken.identifier == ARRAY_PREFIX;
	TagType listType = TAG_END;

	if (specialList) {
		auto character = std::tolower(nextToken.tokenText[0]);
		listType = character == 'b' ? TAG_BYTEARRAY : TAG_INTARRAY;
	} else {
		listType = TAG_LIST;
	}

	Tag list;
	list.name = _name;
	list.type = listType;
	if (listType == TAG_LIST)
		list.listType = nextToken.type;
	_name = "";

	Advance();

	while (_tokens->at(_stepper).identifier != LIST_END && _stepper < _tokens->size()) {
		auto tag = LinkTag(_tokens, _stepper, _name);
		Advance();

		if (tag.type == TAG_END)
			continue;

		if (listType == TAG_LIST) {
			if (list.listType != tag.type)
				Throw("Lists cannot except mismatched types!");
			list.list.push_back(tag);
			continue;
		}

		if (listType == TAG_BYTEARRAY) {
			list.byteArray.push_back(tag.GetByte());
		} else {
			list.intArray.push_back(tag.GetInt());
		}
	}

	return list;
}

// Links together tags into a compound
Tag TextNBTifier::LinkCompound(std::vector<Token>* _tokens, size_t& _stepper, std::string _name) {
	Tag compound;
	compound.type = TAG_COMPOUND;
	compound.name = _name;
	_name = "";

	Advance();

	while (_tokens->at(_stepper).identifier != COMPOUND_END && _stepper < _tokens->size()) {
		auto tag = LinkTag(_tokens, _stepper, _name);
		Advance();
		if (tag.type == TAG_END)
			continue;
		compound.compound[tag.name] = tag;
	}

	if (_stepper > _tokens->size() - 1)
		Throw("Expected TAG_END when linking compound!");

	return compound;
}

// Checks if there is an array identifier
bool TextNBTifier::IsArrayIdentifier(std::vector<Token>* _tokens, size_t& _stepper) {
	const char lowerChar = std::tolower(Peek());
	if (!SkipWhitespace())
		Advance();
	const char higherChar = std::tolower(Peek());
	bool isArrayIdentifier = higherChar == ';' && (lowerChar == 'b' || lowerChar == 'i');
	if (!isArrayIdentifier) {
		_stepper--;
	} else {
		std::string s(2, '\0');
		s[0] = lowerChar;
		s[1] = higherChar;
		_tokens->push_back({ .identifier = ARRAY_PREFIX, .tokenText = s });
		Advance();
	}
	return isArrayIdentifier;
}

// Lexes a string into a series of tokens
void TextNBTifier::Lex(std::vector<Token>* _tokens, size_t& _stepper) {
	while (IndexInBounds(_stepper)) {
		SkipWhitespace();
		LexToken(_tokens, _stepper);
	}
}

// Lex a single token
void TextNBTifier::LexToken(std::vector<Token>* _tokens, size_t& _stepper) {
	SkipWhitespace();

	switch (Peek()) {
	case '\0':
		return;
	case '{':
		LexCompound(_tokens, _stepper);
		return;
	case '[':
		LexList(_tokens, _stepper);
		return;
	case '}':
	case ']':
		return;
	case ':':
		_tokens->push_back({ .identifier = COLON, .tokenText = ":" });
		Advance();
		return;
	case ',':
		_tokens->push_back({ .identifier = COMMA, .tokenText = "," });
		Advance();
		return;
	default:
		LexLiteral(_tokens, _stepper);
		return;
	}
}

// Lex a number or string literal
void TextNBTifier::LexLiteral(std::vector<Token>* _tokens, size_t& /*_stepper*/) {
	TokenIdentifier identifier = NUMBER_LITERAL;
	std::string tokenString;

	const char startChar = Peek();
	const bool quoted = startChar == '"' || startChar == '\'';

	if (quoted) {
		identifier = STRING_LITERAL;
		Advance();

		while (IndexInBounds(stepper)) {
			const char c = Peek();

			if (c == startChar) {
				Advance();
				_tokens->push_back({ .identifier = identifier, .tokenText = tokenString, .type = TAG_STRING });
				return;
			}

			if (c == '\\') {
				Advance();

				if (!IndexInBounds(stepper))
					Throw("Unterminated escape sequence!");

				const char escaped = Peek();

				switch (escaped) {
				case 'n':
					tokenString += '\n';
					break;
				case 'r':
					tokenString += '\r';
					break;
				case 't':
					tokenString += '\t';
					break;
				case '\\':
					tokenString += '\\';
					break;
				case '"':
					tokenString += '"';
					break;
				case '\'':
					tokenString += '\'';
					break;
				default:
					tokenString += escaped;
					break;
				}

				Advance();
				continue;
			}

			tokenString += c;
			Advance();
		}

		Throw("Unterminated string literal!");
	}

	// Read an unquoted literal until a delimiter
	bool isIdentifier = false;
	while (IndexInBounds(stepper)) {
		const char c = Peek();

		if (c == ':' || c == ',' || c == '{' || c == '}' ||
		    c == '[' || c == ']') {
			isIdentifier = c == ':';
			break;
		}

		if (std::isspace(static_cast<unsigned char>(c))) {
			Advance();
			continue;
		}

		tokenString += c;
		Advance();
	}

	if (tokenString.empty())
		Throw("Expected a literal!");

	// Unquoted identifiers are strings unless they resemble numbers
	bool looksNumeric = !isIdentifier;
	bool isDecimal = false;
	bool isTyped = false;
	bool sawDigit = false;
	TagType numericType = TAG_INT;
	if (!isIdentifier) {
		const auto len = tokenString.length();
		for (size_t i = 0; i < len; i++) {
			const char thisChar = tokenString[i];

			if (i == 0 && (thisChar == '-' || thisChar == '+'))
				continue;

			if (thisChar == '.') {
				if (isDecimal) {
					looksNumeric = false;
					break;
				} // "1.2.3"
				isDecimal = true;
				continue;
			}

			if (std::isdigit(static_cast<unsigned char>(thisChar))) {
				sawDigit = true;
				continue;
			}

			// Only the last character may be a type suffix
			if (i == len - 1 && sawDigit) {
				isTyped = true;
				switch (std::tolower(static_cast<unsigned char>(thisChar))) {
				case 'b':
					numericType = TAG_BYTE;
					break;
				case 's':
					numericType = TAG_SHORT;
					break;
				case 'l':
					numericType = TAG_LONG;
					break;
				case 'f':
					numericType = TAG_FLOAT;
					break;
				case 'd':
					numericType = TAG_DOUBLE;
					break;
				default:
					looksNumeric = false;
					break; // "12x" is a string
				}
				break;
			}

			looksNumeric = false;
			break;
		}
	}

	if (!sawDigit)
		looksNumeric = false;

	if (looksNumeric && !isTyped)
		numericType = isDecimal ? TAG_DOUBLE : TAG_INT;

	if (!looksNumeric)
		identifier = STRING_LITERAL;
	else if (isTyped)
		tokenString.pop_back();

	_tokens->push_back(
	    { .identifier = identifier, .tokenText = tokenString, .type = looksNumeric ? numericType : TAG_STRING });
}

// Lex a list recursively
void TextNBTifier::LexList(std::vector<Token>* _tokens, size_t& _stepper) {
	// TODO: isSpecialList is currently not checked anywhere
	//bool isSpecialList = false;
	TagType listType = TAG_END;
	//size_t length = 0;

	if (Peek() != '[') {
		Throw("Tried to parse an invalid list!");
	}

	_tokens->push_back({ .identifier = LIST_BEGIN, .tokenText = std::string(1, Peek()), .type = TAG_LIST });
	Advance();
	SkipWhitespace();
	//isSpecialList = IsArrayIdentifier(_tokens, _stepper);

	while (IndexInBounds(stepper) && Peek() != ']') {
		SkipWhitespace();
		LexToken(_tokens, _stepper);
		if (listType == TAG_END) {
			listType = _tokens->back().type;
		} else {
			if (listType != _tokens->back().type && _tokens->back().type != TAG_END)
				Throw("Multiple types are disallowed in a list!");
		}
	}

	if (Peek() != ']') {
		Throw("Missing closing brace for list!");
	}

	_tokens->push_back({ .identifier = LIST_END, .tokenText = std::string(1, Peek()), .type = TAG_LIST });
	Advance();
}

// Lex a compound recursively
void TextNBTifier::LexCompound(std::vector<Token>* _tokens, size_t& _stepper) {
	if (Peek() != '{') {
		Throw("Tried to parse an invalid compound!");
	}

	_tokens->push_back({ .identifier = COMPOUND_BEGIN, .tokenText = std::string(1, Peek()), .type = TAG_COMPOUND });
	Advance();

	while (IndexInBounds(stepper) && Peek() != '}') {
		SkipWhitespace();
		LexToken(_tokens, _stepper);
	}

	if (Peek() != '}') {
		Throw("Missing closing brace for compound!");
	}

	_tokens->push_back({ .identifier = COMPOUND_END, .tokenText = std::string(1, Peek()), .type = TAG_COMPOUND });
	Advance();
}