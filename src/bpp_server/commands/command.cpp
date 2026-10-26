/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "command.h"
#include "../../bpp_shared/helpers/identifiers.h"
#include "../server.h"
#include "items.h"
#include "command_manager.h"
#include "../entities/entity_mp_player.h"
#include <algorithm>
#include "nbt/nbt_textifier.h"
#include <charconv>
#include <cmath>
#include <limits>
#include <string>

bool IsOperator(PlayerSession& _session, Server& _server) {
	return std::find(_server.operatorUsernames.begin(), _server.operatorUsernames.end(), _session.username) !=
	       _server.operatorUsernames.end();
}

bool IsOperator(const CommandContext& _ctx) {
	return !_ctx.sender || (_ctx.server && IsOperator(*_ctx.sender, *_ctx.server));
}

void SendCommandFeedback(PlayerSession* _recipient, const std::string& _message) {
	if (_recipient) {
		if (!_recipient->stream.IsConnected())
			return;
		SendChat(*_recipient, _message);
		return;
	}
	Logger::LogStyled(_message);
}

void SendFeedback(const CommandContext& _ctx, const std::string& _message) {
	SendCommandFeedback(_ctx.sender, _message);
}

ItemStack ParseItemStack(const std::string& _itemArg, std::optional<int> _count) {
	ItemStack item;

	if (auto it = IDENTIFIER_TO_ID.find(_itemArg); it != IDENTIFIER_TO_ID.end()) {
		item.id = it->second.id;
		item.data = it->second.meta;
	} else {
		const size_t colonPos = _itemArg.find(':');
		const std::string baseString = _itemArg.substr(0, colonPos);

		std::optional<int16_t> explicitMeta;
		if (colonPos != std::string::npos) {
			const std::string metaString = _itemArg.substr(colonPos + 1);
			if (!metaString.empty()) {
				explicitMeta = static_cast<int16_t>(std::stoi(metaString));
			}
		}

		if (auto it2 = IDENTIFIER_TO_ID.find(baseString); it2 != IDENTIFIER_TO_ID.end()) {
			item.id = it2->second.id;
			item.data = it2->second.meta;
		} else {
			item.id = static_cast<int16_t>(std::stoi(baseString));
			item.data = 0;
		}

		// Explicit meta, if given, overrides the default
		if (explicitMeta) {
			item.data = *explicitMeta;
		}
	}

	item.count = _count ? static_cast<int8_t>(*_count) : Items::GetMaxStack(item.id);
	return item;
}

void SendChunkedList(const CommandContext& _ctx, const std::string& _header,
                     const std::vector<std::string>& _entries) {
	SendFeedback(_ctx, _header);
	std::string line = "§7";
	for (size_t i = 0; i < _entries.size(); i++) {
		const auto& entry = _entries[i];
		const std::string suffix = i + 1 < _entries.size() ? ", " : "";
		if (line.size() + entry.size() + suffix.size() > 64 && line != "§7") {
			SendFeedback(_ctx, line);
			line = "§7";
		}
		line += entry + suffix;
	}
	if (line != "§7")
		SendFeedback(_ctx, line);
}

std::optional<std::string> ResolveCmdTargetName(const strategos::CmdNode& _cmd, const CommandContext& _ctx) {
	if (auto name = _cmd.get_arg<std::string>("username"))
		return name;
	if (_ctx.session)
		return _ctx.session->username;
	return std::nullopt;
}

namespace {

struct SelectorArguments {
	std::optional<EntityType> type;
	bool negateType = false;
	std::optional<double> minDistance;
	std::optional<double> maxDistance;
	std::vector<std::pair<Tag, bool>> nbt;
};

std::string_view TrimSelector(std::string_view value) {
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.remove_prefix(1);
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.remove_suffix(1);
	return value;
}

std::optional<EntityType> ParseSelectorEntityType(std::string_view _name) {
	if (_name.starts_with("minecraft:")) _name.remove_prefix(10);
	static constexpr std::pair<std::string_view, EntityType> types[] = {
	    {"item", EntityType::ITEM}, {"player", EntityType::PLAYER}, {"fish", EntityType::FISH},
	    {"fireball", EntityType::FIREBALL}, {"boat", EntityType::BOAT}, {"minecart", EntityType::MINECART},
	    {"storage_minecart", EntityType::STORAGE_MINECART}, {"furnace_minecart", EntityType::FURNACE_MINECART},
	    {"lit_tnt", EntityType::LIT_TNT}, {"arrow", EntityType::ARROW}, {"thrown_snowball", EntityType::THROWN_SNOWBALL},
	    {"thrown_egg", EntityType::THROWN_EGG}, {"falling_sand", EntityType::FALLING_SAND},
	    {"falling_gravel", EntityType::FALLING_GRAVEL}, {"fishing_bobber", EntityType::FISHING_BOBBER},
	    {"creeper", EntityType::CREEPER}, {"skeleton", EntityType::SKELETON}, {"spider", EntityType::SPIDER},
	    {"giant_zombie", EntityType::GIANT_ZOMBIE}, {"zombie", EntityType::ZOMBIE}, {"slime", EntityType::SLIME},
	    {"ghast", EntityType::GHAST}, {"zombie_pigman", EntityType::ZOMBIE_PIGMAN}, {"pig", EntityType::PIG},
	    {"sheep", EntityType::SHEEP}, {"cow", EntityType::COW}, {"chicken", EntityType::CHICKEN},
	    {"squid", EntityType::SQUID}, {"wolf", EntityType::WOLF}, {"painting", EntityType::PAINTING},
	    {"lightning_bolt", EntityType::LIGHTNING_BOLT},
	};

	for (const auto& [name, type] : types) if (name == _name) return type;
	return std::nullopt;
}

// LLM Disclosure (partial, validated)
std::expected<SelectorArguments, std::string> ParseSelectorArguments(std::string_view _arguments) {
	SelectorArguments result;
	if (TrimSelector(_arguments).empty()) return result;

	size_t start = 0;
	while (start < _arguments.size()) {
		size_t end = start, equal = std::string_view::npos;
		std::vector<char> closers;
		char quote = 0;
		bool escaped = false;

		for (; end < _arguments.size(); ++end) {
			const char c = _arguments[end];
			if (quote) {
				if (escaped) escaped = false;
				else if (c == '\\') escaped = true;
				else if (c == quote) quote = 0;
			} else if (c == '"' || c == '\'') quote = c;
			else if (c == '[' || c == '{') closers.push_back(c == '[' ? ']' : '}');
			else if (c == ']' || c == '}') {
				if (closers.empty() || c != closers.back()) return std::unexpected("Malformed selector");
				closers.pop_back();
			} else if (closers.empty()) {
				if (c == ',') break;
				if (c == '=' && equal == std::string_view::npos) equal = end;
			}
		}

		if (quote || !closers.empty() || equal == std::string_view::npos)
			return std::unexpected("Malformed selector");

		auto key = TrimSelector(_arguments.substr(start, equal - start));
		auto value = TrimSelector(_arguments.substr(equal + 1, end - equal - 1));

		if (key.empty() || value.empty()) return std::unexpected("Malformed selector");
		if (key == "type") {
			if (result.type) return std::unexpected("Duplicate selector argument: type");
			if (value.front() == '!') { result.negateType = true; value.remove_prefix(1); }
			result.type = ParseSelectorEntityType(value);
			if (!result.type) return std::unexpected("Unknown selector entity type: " + std::string(value));
		} else if (key == "distance")
		{
			if (result.minDistance || result.maxDistance) return std::unexpected("Duplicate selector argument: distance");
			const auto invalid = [&] { return std::unexpected("Invalid selector distance: " + std::string(value)); };

			auto bound = [](std::string_view text) -> std::optional<double> {
				double number;
				auto [parsedEnd, error] = std::from_chars(text.data(), text.data() + text.size(), number);
				if (error != std::errc{} || parsedEnd != text.data() + text.size() || !std::isfinite(number) || number < 0)
					return std::nullopt;
				return number;
			};
			const auto dots = value.find("..");
			if (value.find("...") != std::string_view::npos) return invalid();
			if (dots == std::string_view::npos) {
				result.minDistance = result.maxDistance = bound(value);
				if (!result.minDistance) return invalid();
			} else {
				const auto min = value.substr(0, dots), max = value.substr(dots + 2);
				if (min.empty() && max.empty()) return invalid();

				if (!min.empty()) {
					result.minDistance = bound(min);
					if (!result.minDistance) return invalid();
				}

				if (!max.empty()) {
					result.maxDistance = bound(max);
					if (!result.maxDistance) return invalid();
				}
				if (result.minDistance && result.maxDistance && *result.minDistance > *result.maxDistance) return invalid();
			}
		} else if (key == "nbt")
		{
			bool negate = value.front() == '!';
			if (negate) value.remove_prefix(1);

			try { result.nbt.emplace_back(TextNBTifier{}.Parse(std::string(value)), negate); }
			catch (const std::runtime_error& error) { return std::unexpected("Invalid selector NBT: " + std::string(error.what()));}
		} else return std::unexpected("Unknown selector argument: " + std::string(key));

		if (end == _arguments.size()) break;
		start = end + 1;
		if (TrimSelector(_arguments.substr(start)).empty()) return std::unexpected("Malformed selector");
	}
	return result;
} // End LLM Disclosure

} // namespace

std::expected<std::vector<CommandTarget>, std::string> ResolveCmdTargets(std::string_view _target,
                                                                        const CommandContext& _ctx) {
	if (_target.empty() || _target.front() != '@') {
		auto player = _ctx.server->GetSessionByUsername(std::string(_target));
		if (!player || !player->entity || player->entity->isDead ||
		    player->connState != ConnectionState::Playing || !player->stream.IsConnected())
			return std::unexpected(std::string(_target) + " is not connected!");

		return std::vector<CommandTarget>{ { player->entity.get(), player.get() } };
	}

	std::string_view selector = _target;
	std::string_view arguments;
	const size_t bracket = selector.find('[');
	if (bracket != std::string_view::npos) {
		if (selector.back() != ']') return std::unexpected("Malformed selector");
		arguments = selector.substr(bracket + 1, selector.size() - bracket - 2);
		selector = selector.substr(0, bracket);
	} else if (selector.find(']') != std::string_view::npos || selector.find('{') != std::string_view::npos ||
	           selector.find('}') != std::string_view::npos) return std::unexpected("Malformed selector");

	const bool known = selector == "@p" || selector == "@r" || selector == "@a" ||
	                   selector == "@e" || selector == "@s" || selector == "@n";

	if (!known) return std::unexpected("Unknown selector");
	auto filters = ParseSelectorArguments(arguments);
	if (!filters) return std::unexpected(filters.error());
	if (!_ctx.world) return std::vector<CommandTarget>{};

	const auto fromEntity = [](Entity* _entity) {
		auto* player = dynamic_cast<EntityMPPlayer*>(_entity);
		return CommandTarget{ _entity, player ? player->session : nullptr };
	};

	std::vector<CommandTarget> eligible;
	if (selector == "@a" || selector == "@r" || selector == "@p") {
		for (const auto& session : _ctx.server->GetPlayers()) {
			if (!session || !session->stream.IsConnected() ||
				session->connState != ConnectionState::Playing ||
			    !session->entity || session->entity->isDead ||
			    _ctx.server->GetWorldForDimension(session->dimension) != _ctx.world)
				continue;
			eligible.push_back({ session->entity.get(), session.get() });
		}
	} else if (selector == "@e" || selector == "@n") {
		for (const auto& entity : _ctx.world->entityManager.entities)
			if (entity && !entity->isDead) eligible.push_back(fromEntity(entity.get()));
	} else if (selector == "@s" && _ctx.executionEntity && !_ctx.executionEntity->isDead &&
	           _ctx.executionEntity->world == _ctx.world) {
		eligible.push_back(fromEntity(_ctx.executionEntity));
	}

	const Vec3 origin = _ctx.executionEntity ? _ctx.executionEntity->position : Vec3{};
	const auto distance = [&origin](Entity* entity) {
		const double x = entity->position.x - origin.x;
		const double y = entity->position.y - origin.y;
		const double z = entity->position.z - origin.z;
		return x * x + y * y + z * z;
	};

	const double squareLimit = std::sqrt(std::numeric_limits<double>::max());
	std::erase_if(eligible, [&](const CommandTarget& target) {
		if (filters->type && ((target.entity->type == *filters->type) == filters->negateType)) return true;
		if (filters->minDistance || filters->maxDistance) {
			const double squared = distance(target.entity);
	
			if (filters->minDistance &&
			    (*filters->minDistance > squareLimit || squared < *filters->minDistance * *filters->minDistance))
				return true;
			if (filters->maxDistance && *filters->maxDistance <= squareLimit &&
			    squared > *filters->maxDistance * *filters->maxDistance) return true;
		}

		if (!filters->nbt.empty()) {
			std::optional<Tag> actual;

			if (target.entity->entityManager) {
				if (target.session) actual = target.session->SerializeToNbt();
				else actual = target.entity->SerializeToNbt();
			}

			const bool represented = actual && actual->type == TAG_COMPOUND;

			for (const auto& [pattern, negate] : filters->nbt) {
				const bool matches = represented && actual->Matches(pattern);
				if (matches == negate) return true;
			}
		}
		return false;
	});

	if (selector == "@r") {
		if (eligible.empty())
			return eligible;
		auto* tracker = _ctx.server->GetEntityTrackerForDimension(_ctx.world->thisDimension);
		return std::vector<CommandTarget>{ eligible[static_cast<size_t>(tracker->rand.NextInt(
			static_cast<int32_t>(eligible.size())))] };
	}

	if (selector == "@p" || selector == "@n") {
		std::sort(eligible.begin(), eligible.end(), [&distance](const CommandTarget& _a, const CommandTarget& _b) {
			const double da = distance(_a.entity), db = distance(_b.entity);
			return da == db ? _a.entity->id < _b.entity->id : da < db;
		});
		if (eligible.size() > 1) eligible.resize(1);
	}
	return eligible;
}

std::expected<Vec3, std::string> ResolveCmdPos(const CommandContext& _ctx,
                                              const strategos::Vec3& _position) {
	const Vec3 origin = _ctx.executionEntity ? _ctx.executionEntity->position : Vec3{};
	return ResolveCmdVec3(_position, origin);
}

