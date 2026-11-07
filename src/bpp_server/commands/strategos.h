/*
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

// A modified version of Strategos by jwaxy
// https://gist.github.com/jwaxy/3e92259f20ef69ebb62528801ccefcb5#file-strategos-cpp

#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <expected>
#include <format>
#include <functional>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace strategos {

struct CmdNode;
struct Node;

enum class NodeType : uint8_t {
	Literal = 0,
	Root,
	String,
	GreedyString,
	Integer,
	Float,
	Boolean,
	Enum,
	Vec3,
};

enum class Axis : uint8_t {
	X,
	Y,
	Z
};

struct Vec3 {
	float pos[3]{};
	uint8_t relative_axes = 0; // bitmask

	[[nodiscard]] constexpr auto is_relative(Axis axis) const noexcept -> bool {
		return relative_axes & (1 << std::to_underlying(axis));
	}

	constexpr void set_relative(Axis axis, bool rel = true) noexcept {
		if (rel)
			relative_axes |= (1 << std::to_underlying(axis));
		else
			relative_axes &= ~(1 << std::to_underlying(axis));
	}
};
} // namespace strategos

namespace std {

template <>
struct formatter<strategos::Vec3> : formatter<std::string> {
	auto format(const strategos::Vec3& v, format_context& ctx) const {
		return formatter<std::string>::format(std::format("[{}, {}, {}]", v.pos[0], v.pos[1], v.pos[2]), ctx);
	}
};

} // namespace std

namespace strategos {

using ActionFunc = std::function<std::string(const CmdNode& ctx, void* user_data)>;
using PermissionFunc = std::function<bool(void* user_data)>;

struct IntSettings {
	int min = std::numeric_limits<int>::min();
	int max = std::numeric_limits<int>::max();
};

struct FloatSettings {
	float min = std::numeric_limits<float>::lowest();
	float max = std::numeric_limits<float>::max();
};

struct EnumSettings {
	std::vector<std::string> values;
};

enum class SuggestionSource : uint8_t {
	None,
	Players,
	Commands,
	NestedCommand,
};

struct Suggestion {
	size_t start;
	std::string text;
};

// Settings are used to specify metadata like constraints for a node.
using NodeSettings = std::variant<std::monostate, IntSettings, FloatSettings, EnumSettings>;

struct Node {
	NodeType type{};
	std::string name;
	NodeSettings settings;
	ActionFunc action;
	std::string description;
	bool requiresOperator = false;
	SuggestionSource suggestionSource = SuggestionSource::None;

	// catchall string nodes follow typed or literal children so matches win.
	std::vector<Node> children;

	Node& executes(ActionFunc fn) {
		action = std::move(fn);
		return *this;
	}

	Node& describe(std::string desc) {
		description = std::move(desc);
		return *this;
	}

	Node& op(bool required = true) {
		requiresOperator = required;
		return *this;
	}

	Node& suggests(SuggestionSource source) {
		suggestionSource = source;
		return *this;
	}

	Node& then(Node child) {
		if (child.type == NodeType::String || child.type == NodeType::GreedyString) {
			children.push_back(std::move(child));
		} else {
			auto it = std::find_if(children.begin(), children.end(), [](const Node& n) {
				return n.type == NodeType::String || n.type == NodeType::GreedyString;
			});
			children.insert(it, std::move(child));
		}
		return *this;
	}

	static inline Node literal(std::string name) {
		return { NodeType::Literal, std::move(name) };
	}

	static inline Node string(std::string name) {
		return { NodeType::String, std::move(name) };
	}

	static inline Node greedy_string(std::string name) {
		return { NodeType::GreedyString, std::move(name) };
	}

	static inline Node integer(std::string name, int min, int max) {
		return { NodeType::Integer, std::move(name), IntSettings{ min, max } };
	}

	static inline Node integer(std::string name) {
		return { NodeType::Integer, std::move(name), IntSettings{} };
	}

	static inline Node float_(std::string name, float min, float max) {
		return { NodeType::Float, std::move(name), FloatSettings{ min, max } };
	}

	static inline Node float_(std::string name) {
		return { NodeType::Float, std::move(name), FloatSettings{} };
	}

	static inline Node boolean(std::string name) {
		return { NodeType::Boolean, std::move(name) };
	}

	static inline Node vec3(std::string name) {
		return { NodeType::Vec3, std::move(name) };
	}

	static inline Node enum_(std::string name, std::vector<std::string> values) {
		return { NodeType::Enum, std::move(name), EnumSettings{ std::move(values) } };
	}
};

using CmdNodeData = std::variant<std::monostate, int, float, std::string, Vec3, bool>;

struct CmdNode {
	const Node* node{ nullptr };
	CmdNodeData data;
	const CmdNode* next{ nullptr };

	template <typename T>
	[[nodiscard]] auto get() const noexcept -> std::optional<T> {
		if (auto* val = std::get_if<T>(&data)) {
			return *val;
		}
		return std::nullopt;
	}

	[[nodiscard]] auto get_child(size_t index) const noexcept -> const CmdNode* {
		auto* current = this;
		for (size_t i = 0; i < index && current; ++i) {
			current = current->next;
		}
		return current;
	}

	template <typename T>
	[[nodiscard]] auto get_arg(size_t index) const noexcept -> std::optional<T> {
		if (auto* child = get_child(index)) {
			return child->get<T>();
		}
		return std::nullopt;
	}

	template <typename T>
	[[nodiscard]] auto get_arg(std::string_view name) const noexcept -> std::optional<T> {
		for (auto* current = this; current; current = current->next) {
			if (current->node && current->node->name == name) {
				return current->get<T>();
			}
		}
		return std::nullopt;
	}
};

enum class ParseError : uint8_t {
	InvalidToken,
	OutOfRange,
	IncompleteCommand,
	TooManyArguments,
	UnknownCommand,
	NoPermission,
};

struct ErrorInfo {
	ParseError error;
	std::string message;
	size_t position{ 0 };
};

using ActionResult = std::expected<std::string, ErrorInfo>;

class BrigadierContext {
private:
	Node rootNode;

public:
	BrigadierContext() {
		rootNode.type = NodeType::Root;
	}

	BrigadierContext& add_command(Node cmd) {
		rootNode.then(std::move(cmd));
		return *this;
	}

	[[nodiscard]] const Node& root() const {
		return rootNode;
	}

	// LLM disclosure
	[[nodiscard]] auto suggest(std::string_view input, bool canOperate,
	                           std::span<const std::string> players) const -> std::vector<Suggestion> {
		size_t base = 0;
		while (base < input.size() && input[base] == ' ')
			++base;
		if (base < input.size() && input[base] == '/') {
			++base;
			while (base < input.size() && input[base] == ' ')
				++base;
		}
		const auto command = input.substr(base);
		auto tokens = tokenize(command);
		const bool trailingSpace = !command.empty() && command.back() == ' ' &&
		                           (tokens.empty() || tokens.back().data() + tokens.back().size() != command.data() + command.size());
		std::vector<size_t> offsets;
		offsets.reserve(tokens.size());
		for (auto token : tokens)
			offsets.push_back(base + static_cast<size_t>(token.data() - command.data()));
		std::string_view prefix;
		size_t replaceStart = input.size();
		size_t completed = tokens.size();
		if (!trailingSpace && !tokens.empty()) {
			prefix = tokens.back();
			replaceStart = offsets.back();
			--completed;
		}
		auto result = suggest_at(rootNode, tokens, offsets, completed, prefix, replaceStart, canOperate, players, 0);
		std::ranges::sort(result, {}, &Suggestion::text);
		result.erase(std::unique(result.begin(), result.end(), [](const Suggestion& lhs, const Suggestion& rhs) {
			return lhs.start == rhs.start && lhs.text == rhs.text;
		}), result.end());
		return result;
	}
	// End LLM disclosure

	[[nodiscard]] auto execute(std::string_view cmdline, void* user_data = nullptr,
	                           PermissionFunc canOperate = nullptr) const -> ActionResult {
		auto tokens = tokenize(cmdline);
		if (tokens.empty()) {
			return std::unexpected(ErrorInfo{ .error = ParseError::IncompleteCommand, .message = "Empty command" });
		}

		std::vector<CmdNode> cmd_ctx;
		// Pre-allocating to prevent pointer invalidation for CmdNode::next links
		cmd_ctx.reserve(tokens.size() + 1);

		cmd_ctx.push_back({ .node = &rootNode });

		const Node* current = &rootNode;
		size_t token_idx = 0;

		auto lacksOperator = [&]() {
			return cmd_ctx.size() > 1 && cmd_ctx[1].node && cmd_ctx[1].node->requiresOperator &&
			       (!canOperate || !canOperate(user_data));
		};
		auto operatorError = []() {
			return std::unexpected(
			    ErrorInfo{ .error = ParseError::NoPermission, .message = "Only operators can use this command!" });
		};

		while (token_idx < tokens.size()) {
			if (current->children.empty()) {
				if (lacksOperator())
					return operatorError();
				return std::unexpected(ErrorInfo{ .error = ParseError::TooManyArguments,
				                                  .message = std::format("Unexpected token '{}'", tokens[token_idx]),
				                                  .position = token_idx });
			}

			auto [matched_node, consumed] = find_matching_child(*current, tokens, token_idx);

			if (!matched_node) {
				if (lacksOperator())
					return operatorError();
				return std::unexpected(
				    ErrorInfo{ .error = ParseError::UnknownCommand,
				               .message = std::format("Unknown token '{}'. Expected: {}", tokens[token_idx],
				                                      get_expected_names(*current)),
				               .position = token_idx });
			}

			auto& cmd_node = cmd_ctx.emplace_back();
			cmd_node.node = matched_node;

			// Privilege check as soon as the command is identified, before syntax errors.
			if (lacksOperator())
				return operatorError();

			if (!parse_node_value(*matched_node, tokens, token_idx, consumed, cmdline, cmd_node.data)) {
				return std::unexpected(ErrorInfo{ .error = ParseError::InvalidToken,
				                                  .message = std::format("Invalid value for '{}'", matched_node->name),
				                                  .position = token_idx });
			}

			cmd_ctx[cmd_ctx.size() - 2].next = &cmd_node;

			current = matched_node;
			token_idx += 1 + consumed;
		}

		if (lacksOperator())
			return operatorError();

		if (!current->action) {
			return std::unexpected(
			    ErrorInfo{ .error = ParseError::IncompleteCommand,
			               .message = std::format("Incomplete command. Expected: {}", get_expected_names(*current)) });
		}

		return current->action(cmd_ctx[1], user_data);
	}

private:
	// LLM Disclosure
	[[nodiscard]] auto suggest_at(const Node& root, std::span<const std::string_view> tokens,
	                             std::span<const size_t> offsets, size_t completed, std::string_view prefix,
	                             size_t replaceStart, bool canOperate, std::span<const std::string> players,
	                             unsigned depth) const -> std::vector<Suggestion> {
		std::vector<Suggestion> output;
		const Node* current = &root;
		size_t index = 0;
		while (index < completed) {
			auto [node, consumed] = find_matching_child(*current, tokens.first(completed), index);
			if (!node)
				return output;
			if (node->type == NodeType::Vec3 && consumed != 2)
				return output;
			if (node->type == NodeType::GreedyString && node->suggestionSource == SuggestionSource::NestedCommand) {
				if (depth < 16)
					return suggest_nested(tokens.subspan(index), offsets.subspan(index), completed - index, prefix,
					                      replaceStart, canOperate, players, depth + 1);
				return output;
			}
			if (node->type != NodeType::GreedyString) {
				CmdNodeData ignored;
				if (!parse_node_value(*node, tokens.first(completed), index, consumed, {}, ignored))
					return output;
			}
			if (index == 0 && node->requiresOperator && !canOperate)
				return output;
			current = node;
			index += 1 + consumed;
		}

		for (const auto& child : current->children) {
			if (index == 0 && child.requiresOperator && !canOperate)
				continue;
			if (child.type == NodeType::Literal) {
				append_candidate(output, child.name, prefix, replaceStart);
				if (!prefix.empty() && child.name == prefix)
					break;
				continue;
			}
			const bool typedMatch = !prefix.empty() && child.type != NodeType::String &&
			                        child.type != NodeType::GreedyString && matches_node_type(child, prefix);
			if (child.type == NodeType::Enum) {
				for (const auto& value : std::get<EnumSettings>(child.settings).values)
					append_candidate(output, value, prefix, replaceStart);
			} else if (child.type == NodeType::Boolean) {
				append_candidate(output, "true", prefix, replaceStart);
				append_candidate(output, "false", prefix, replaceStart);
			}
			switch (child.suggestionSource) {
			case SuggestionSource::Players:
				for (const auto& player : players)
					append_candidate(output, player, prefix, replaceStart);
				break;
			case SuggestionSource::Commands:
				for (const auto& command : rootNode.children)
					if (!command.requiresOperator || canOperate)
						append_candidate(output, command.name, prefix, replaceStart);
				break;
			case SuggestionSource::NestedCommand:
				if (child.type == NodeType::GreedyString && depth < 16) {
					auto nested = suggest_nested({}, {}, 0, prefix, replaceStart, canOperate, players, depth + 1);
					output.insert(output.end(), nested.begin(), nested.end());
				}
				break;
			case SuggestionSource::None:
				break;
			}
			if (typedMatch)
				break;
		}
		return output;
	}

	[[nodiscard]] auto suggest_nested(std::span<const std::string_view> tokens,
	                                 std::span<const size_t> offsets, size_t completed, std::string_view prefix,
	                                 size_t replaceStart, bool canOperate, std::span<const std::string> players,
	                                 unsigned depth) const -> std::vector<Suggestion> {
		std::vector<std::string_view> normalizedTokens(tokens.begin(), tokens.end());
		std::vector<size_t> normalizedOffsets(offsets.begin(), offsets.end());
		if (!prefix.empty() && prefix.front() == '/') {
			prefix.remove_prefix(1);
			++replaceStart;
		} else if (completed > 0 && !normalizedTokens.empty() && normalizedTokens.front().starts_with('/')) {
			normalizedTokens.front().remove_prefix(1);
			if (normalizedTokens.front().empty()) {
				normalizedTokens.erase(normalizedTokens.begin());
				if (!normalizedOffsets.empty())
					normalizedOffsets.erase(normalizedOffsets.begin());
				--completed;
			}
		}
		return suggest_at(rootNode, normalizedTokens, normalizedOffsets, completed, prefix, replaceStart,
		                  canOperate, players, depth);
	}

	static void append_candidate(std::vector<Suggestion>& output, std::string_view value,
	                             std::string_view prefix, size_t start) {
		if (value.starts_with(prefix))
			output.push_back({ start, std::string(value) });
	} // End LLM disclosure

	[[nodiscard]] static auto tokenize(std::string_view input) -> std::vector<std::string_view> {
		std::vector<std::string_view> tokens;
		for (size_t pos = 0; pos < input.size();) {
			while (pos < input.size() && input[pos] == ' ') ++pos;
			if (pos == input.size()) break;

			const size_t start = pos;
			const bool selector = input[pos] == '@';
			std::vector<char> closers;
			char quote = 0;
			bool escaped = false;
			bool malformed = false;

			for (; pos < input.size(); ++pos) {
				const char c = input[pos];
				if (quote) {
					if (escaped) escaped = false;
					else if (c == '\\') escaped = true;
					else if (c == quote) quote = 0;
					continue;
				}

				if (!closers.empty()) {
					if (c == '"' || c == '\'') quote = c;
					else if (c == '[') closers.push_back(']');
					else if (c == '{') closers.push_back('}');
					else if (c == ']' || c == '}') {
						if (c != closers.back()) malformed = true;
						else closers.pop_back();
					}
				} else if (selector && c == '[') closers.push_back(']');

				else if (c == ' ' && !malformed) break;
			}
			tokens.push_back(input.substr(start, pos - start));
		}

		return tokens;
	}

	[[nodiscard]] static auto find_matching_child(const Node& parent, std::span<const std::string_view> tokens,
	                                              size_t token_idx) -> std::pair<const Node*, size_t> {
		const auto& token = tokens[token_idx];

		for (const auto& child : parent.children) {
			if (matches_node_type(child, token)) {
				size_t consumed = get_consumed_tokens(child, tokens, token_idx);
				return { &child, consumed };
			}
		}

		return { nullptr, 0 };
	}

	[[nodiscard]] static auto matches_node_type(const Node& node, std::string_view token) -> bool {
		switch (node.type) {
		case NodeType::Literal:
			return node.name == token;

		case NodeType::String:
		case NodeType::GreedyString:
			return true;

		case NodeType::Integer:
			return validate_int(token);

		case NodeType::Float:
			return validate_float(token);

		case NodeType::Boolean:
			return token == "true" || token == "false";

		case NodeType::Enum: {
			const auto& settings = std::get<EnumSettings>(node.settings);
			return std::ranges::find(settings.values, token) != settings.values.end();
		}

		case NodeType::Vec3:
			return validate_vec3_component(token);

		default:
			return false;
		}
	}

	[[nodiscard]] static auto get_consumed_tokens(const Node& node, std::span<const std::string_view> tokens,
	                                              size_t token_idx) -> size_t {
		if (node.type == NodeType::GreedyString)
		return tokens.size() - token_idx - 1;
		if (node.type == NodeType::Vec3 && tokens.size() > token_idx + 2)
			return 2;
		return 0;
	}

	[[nodiscard]] static auto parse_node_value(const Node& node, std::span<const std::string_view> tokens,
	                                           size_t token_idx, size_t consumed, std::string_view original,
	                                           CmdNodeData& data) -> bool {
		const auto& token = tokens[token_idx];

		switch (node.type) {
		case NodeType::Literal:
			data = std::monostate{};
			return true;

		case NodeType::String:
			data = std::string(token);
			return true;
			
		case NodeType::GreedyString: {
			const size_t offset = static_cast<size_t>(token.data() - original.data());
			data = std::string(original.substr(offset));
			return true;
		}

		case NodeType::Integer: {
			if (auto val = parse_int(token)) {
				IntSettings settings{};
				if (auto* s = std::get_if<IntSettings>(&node.settings)) {
					settings = *s;
				}
				if (*val < settings.min || *val > settings.max) {
					return false;
				}
				data = *val;
				return true;
			}
			return false;
		}

		case NodeType::Float: {
			if (auto val = parse_float(token)) {
				FloatSettings settings{};
				if (auto* s = std::get_if<FloatSettings>(&node.settings)) {
					settings = *s;
				}
				if (*val < settings.min || *val > settings.max) {
					return false;
				}
				data = *val;
				return true;
			}
			return false;
		}

		case NodeType::Boolean:
			data = (token == "true");
			return true;

		case NodeType::Vec3: {
			if (consumed != 2)
				return false;
			Vec3 vec;
			if (!parse_vec3_component(token, vec, Axis::X))
				return false;
			if (!parse_vec3_component(tokens[token_idx + 1], vec, Axis::Y))
				return false;
			if (!parse_vec3_component(tokens[token_idx + 2], vec, Axis::Z))
				return false;
			data = vec;
			return true;
		}

		case NodeType::Enum: {
			const auto& settings = std::get<EnumSettings>(node.settings);
			auto it = std::ranges::find(settings.values, token);
			if (it != settings.values.end()) {
				data = static_cast<int>(std::distance(settings.values.begin(), it));
				return true;
			}
			return false;
		}

		default:
			return false;
		}
	}

	[[nodiscard]] static auto validate_int(std::string_view token) -> bool {
		return parse_int(token).has_value();
	}

	[[nodiscard]] static auto validate_float(std::string_view token) -> bool {
		return parse_float(token).has_value();
	}

	[[nodiscard]] static auto parse_int(std::string_view token) -> std::optional<int> {
		if (token.empty())
			return std::nullopt;

		size_t pos = 0;
		if (token[0] == '-' || token[0] == '+') {
			pos = 1;
		}

		if (pos >= token.size())
			return std::nullopt;

		for (size_t i = pos; i < token.size(); ++i) {
			if (!std::isdigit(static_cast<unsigned char>(token[i]))) {
				return std::nullopt;
			}
		}

		try {
			return std::stoi(std::string(token));
		} catch (...) {
			return std::nullopt;
		}
	}

	[[nodiscard]] static auto parse_float(std::string_view token) -> std::optional<float> {
		if (token.empty())
			return std::nullopt;

		try {
			size_t pos = 0;
			float val = std::stof(std::string(token), &pos);
			return (pos == token.size()) ? std::optional{ val } : std::nullopt;
		} catch (...) {
			return std::nullopt;
		}
	}

	[[nodiscard]] static auto validate_vec3_component(std::string_view token) -> bool {
		if (token.empty())
			return false;

		auto t = token;
		if (t[0] == '~') {
			t = t.substr(1);
			if (t.empty())
				return true; // Just '~' is valid
		}

		return validate_float(t);
	}

	[[nodiscard]] static auto parse_vec3_component(std::string_view token, Vec3& vec, Axis axis) -> bool {
		auto t = token;
		bool relative = false;
		float value = 0.0f;

		if (t[0] == '~') {
			relative = true;
			t = t.substr(1);
			if (!t.empty()) {
				if (auto val = parse_float(t)) {
					value = *val;
				} else {
					return false;
				}
			}
		} else {
			if (auto val = parse_float(t)) {
				value = *val;
			} else {
				return false;
			}
		}

		vec.pos[std::to_underlying(axis)] = value;
		vec.set_relative(axis, relative);
		return true;
	}

	[[nodiscard]] static auto get_expected_names(const Node& node) -> std::string {
		std::string result;

		for (const auto& child : node.children) {
			if (!result.empty())
				result += ", ";

			switch (child.type) {
			case NodeType::Literal:
				result += child.name;
				break;
			default:
				result += std::format("<{}:{}>", child.name, node_type_to_string(child.type));
			}
		}

		return result.empty() ? "<end>" : result;
	}

	[[nodiscard]] static auto node_type_to_string(NodeType type) -> std::string_view {
		using enum NodeType;
		switch (type) {
		case Literal:
			return "literal";
		case Root:
			return "root";
		case String:
			return "string";
		case Integer:
			return "integer";
		case Float:
			return "float";
		case Boolean:
			return "boolean";
		case Enum:
			return "enum";
		case Vec3:
			return "vec3";
		default:
			return "unknown";
		}
	}
};

} // namespace strategos
