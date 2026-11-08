#include "LevelString.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iterator>

namespace objrep {

namespace {

	constexpr double PI = 3.14159265358979323846;

	constexpr std::string_view KEY_ID = "1";
	constexpr std::string_view KEY_X = "2";
	constexpr std::string_view KEY_Y = "3";
	constexpr std::string_view KEY_FLIP_X = "4";
	constexpr std::string_view KEY_FLIP_Y = "5";
	constexpr std::string_view KEY_ROTATION = "6";

	// Groups, multiple ids separated by '.'.
	constexpr std::string_view KEY_GROUPS = "57";

	// Editor layer (20, 61) and z information (24, 25).
	constexpr std::string_view KEYS_LAYER[] = { "20", "61", "24", "25" };

	// Color channels (21, 22) and the legacy color property (19), plus the HSV
	// companions (41 - 44).
	constexpr std::string_view KEYS_COLOR[] = { "19", "21", "22", "41", "42", "43", "44" };

	bool isDigit(char c) {
		return c >= '0' && c <= '9';
	}

	// Copies every key that the target has onto the replacement. When
	// `removeFirst` is set the replacement's own value is dropped instead of
	// being kept, so a target without the property also ends up with the
	// default value.
	void copyKeys(
		props::List const& from, props::List& to,
		std::string_view const* keys, size_t count, bool removeFirst
	) {
		for (size_t i = 0; i < count; ++i) {
			if (removeFirst) {
				props::erase(to, keys[i]);
			}

			auto value = props::find(from, keys[i]);
			if (!value.empty()) {
				props::set(to, keys[i], std::string(value));
			}
		}
	}

	// Turns one replacement object into the final object string of a specific
	// target object.
	bool buildReplacement(
		CapturedObject const& object, props::List const& target,
		RewriteSettings const& settings, std::string& out
	) {
		auto replacement = props::parse(object.props);
		if (replacement.empty() || props::find(replacement, KEY_ID).empty()) {
			return false;
		}

		double const targetX = props::number(target, KEY_X);
		double const targetY = props::number(target, KEY_Y);
		double const targetRotation = props::number(target, KEY_ROTATION);
		double const targetScaleX = props::scaleX(target);
		double const targetScaleY = props::scaleY(target);
		bool const targetFlipX = props::flag(target, KEY_FLIP_X);
		bool const targetFlipY = props::flag(target, KEY_FLIP_Y);

		double dx = object.dx;
		double dy = object.dy;

		if (settings.transform) {
			dx *= targetScaleX;
			dy *= targetScaleY;

			if (targetFlipX) dx = -dx;
			if (targetFlipY) dy = -dy;

			// GD rotations are clockwise on screen while level Y grows upwards,
			// so the offsets rotate by the negated angle.
			double const radians = -targetRotation * PI / 180.0;
			double const cosR = std::cos(radians);
			double const sinR = std::sin(radians);

			double const rotatedX = dx * cosR - dy * sinR;
			double const rotatedY = dx * sinR + dy * cosR;
			dx = rotatedX;
			dy = rotatedY;
		}

		props::set(replacement, KEY_X, props::formatNumber(targetX + dx));
		props::set(replacement, KEY_Y, props::formatNumber(targetY + dy));

		double rotation = props::number(replacement, KEY_ROTATION);
		if (settings.transform) {
			rotation += targetRotation;
		}

		if (std::fabs(rotation) < 0.0001) {
			props::erase(replacement, KEY_ROTATION);
		}
		else {
			props::set(replacement, KEY_ROTATION, props::formatNumber(rotation));
		}

		double scaleX = props::scaleX(replacement);
		double scaleY = props::scaleY(replacement);
		if (settings.transform) {
			scaleX *= targetScaleX;
			scaleY *= targetScaleY;
		}
		props::setScale(replacement, scaleX, scaleY);

		if (settings.transform) {
			bool const flipX = props::flag(replacement, KEY_FLIP_X) != targetFlipX;
			bool const flipY = props::flag(replacement, KEY_FLIP_Y) != targetFlipY;

			if (flipX) {
				props::set(replacement, KEY_FLIP_X, "1");
			}
			else {
				props::erase(replacement, KEY_FLIP_X);
			}

			if (flipY) {
				props::set(replacement, KEY_FLIP_Y, "1");
			}
			else {
				props::erase(replacement, KEY_FLIP_Y);
			}
		}

		if (settings.inheritGroups) {
			// Only take over the groups when the target actually has some, so a
			// replacement built out of grouped objects keeps working.
			auto groups = props::find(target, KEY_GROUPS);
			if (!groups.empty()) {
				props::set(replacement, KEY_GROUPS, std::string(groups));
			}
		}

		if (settings.inheritColor) {
			bool const targetHasColor =
				!props::find(target, "19").empty() ||
				!props::find(target, "21").empty() ||
				!props::find(target, "22").empty();

			if (targetHasColor) {
				copyKeys(target, replacement, KEYS_COLOR, std::size(KEYS_COLOR), true);
			}
		}

		if (settings.inheritLayer) {
			copyKeys(target, replacement, KEYS_LAYER, std::size(KEYS_LAYER), true);
		}

		out += props::serialize(replacement);
		return true;
	}

	Rule const* findRule(std::vector<Rule> const& rules, int objectID) {
		for (auto const& rule : rules) {
			if (rule.enabled && rule.targetID == objectID && !rule.objects.empty()) {
				return &rule;
			}
		}
		return nullptr;
	}

}

// ---------------------------------------------------------------------------
// object property helpers
// ---------------------------------------------------------------------------

namespace props {

	List parse(std::string_view objectString) {
		List out;

		size_t pos = 0;
		std::string key;
		bool haveKey = false;

		while (pos <= objectString.size()) {
			size_t const comma = objectString.find(',', pos);
			std::string_view const token = objectString.substr(
				pos, (comma == std::string_view::npos ? objectString.size() : comma) - pos
			);

			if (!haveKey) {
				key.assign(token);
				haveKey = true;
			}
			else {
				out.emplace_back(std::move(key), std::string(token));
				key.clear();
				haveKey = false;
			}

			if (comma == std::string_view::npos) {
				break;
			}
			pos = comma + 1;
		}

		return out;
	}

	std::string serialize(List const& list) {
		std::string out;
		for (size_t i = 0; i < list.size(); ++i) {
			if (i) out += ',';
			out += list[i].first;
			out += ',';
			out += list[i].second;
		}
		return out;
	}

	std::string_view find(List const& list, std::string_view key) {
		for (auto const& entry : list) {
			if (entry.first == key) {
				return entry.second;
			}
		}
		return {};
	}

	void set(List& list, std::string_view key, std::string value) {
		for (auto& entry : list) {
			if (entry.first == key) {
				entry.second = std::move(value);
				return;
			}
		}
		list.emplace_back(std::string(key), std::move(value));
	}

	void erase(List& list, std::string_view key) {
		list.erase(
			std::remove_if(list.begin(), list.end(), [key](auto const& entry) {
				return entry.first == key;
			}),
			list.end()
		);
	}

	double number(List const& list, std::string_view key, double fallback) {
		auto const value = find(list, key);
		if (value.empty()) {
			return fallback;
		}
		return std::strtod(std::string(value).c_str(), nullptr);
	}

	bool flag(List const& list, std::string_view key) {
		auto const value = find(list, key);
		if (value.empty()) {
			return false;
		}
		return std::strtod(std::string(value).c_str(), nullptr) != 0.0;
	}

	double scaleX(List const& list) {
		if (!find(list, "128").empty()) return number(list, "128", 1.0);
		if (!find(list, "32").empty()) return number(list, "32", 1.0);
		return 1.0;
	}

	double scaleY(List const& list) {
		if (!find(list, "129").empty()) return number(list, "129", 1.0);
		if (!find(list, "32").empty()) return number(list, "32", 1.0);
		return 1.0;
	}

	void setScale(List& list, double scaleX, double scaleY) {
		// GD itself always saves scale as 128 (x) and 129 (y) and only when the
		// value is not 1. Key 32 is read as a fallback but never written, so a
		// rewritten object can never end up with the scale applied twice.
		props::erase(list, "32");

		if (std::fabs(scaleX - 1.0) < 0.0001) {
			props::erase(list, "128");
		}
		else {
			props::set(list, "128", formatNumber(scaleX));
		}

		if (std::fabs(scaleY - 1.0) < 0.0001) {
			props::erase(list, "129");
		}
		else {
			props::set(list, "129", formatNumber(scaleY));
		}
	}

	std::string formatNumber(double value) {
		if (std::fabs(value - std::round(value)) < 0.0005) {
			return std::to_string(static_cast<long long>(std::llround(value)));
		}

		char buffer[64];
		std::snprintf(buffer, sizeof(buffer), "%.4f", value);

		std::string out = buffer;
		while (!out.empty() && out.back() == '0') {
			out.pop_back();
		}
		if (!out.empty() && out.back() == '.') {
			out.pop_back();
		}
		if (out == "-0") {
			out = "0";
		}
		return out;
	}

}

int objectIDOf(std::string_view objectLine) {
	// Objects start with "1,<id>," - the first key is always the object ID, and
	// anything that does not start with a digit is the level settings header.
	if (objectLine.empty() || !isDigit(objectLine.front())) {
		return 0;
	}

	size_t const firstComma = objectLine.find(',');
	if (firstComma == std::string_view::npos || objectLine.substr(0, firstComma) != "1") {
		return 0;
	}

	size_t const secondComma = objectLine.find(',', firstComma + 1);
	size_t const end = (secondComma == std::string_view::npos) ? objectLine.size() : secondComma;

	std::string const id(objectLine.substr(firstComma + 1, end - firstComma - 1));
	return std::atoi(id.c_str());
}

std::string rewriteLevelString(
	std::string const& levelString,
	std::vector<Rule> const& rules,
	RewriteSettings const& settings
) {
	if (levelString.empty() || rules.empty()) {
		return levelString;
	}

	std::string out;
	out.reserve(levelString.size() + levelString.size() / 8);

	size_t pos = 0;

	while (true) {
		size_t const semicolon = levelString.find(';', pos);
		size_t const lineEnd = (semicolon == std::string::npos) ? levelString.size() : semicolon;
		std::string_view const line(levelString.data() + pos, lineEnd - pos);

		bool expanded = false;

		int const objectID = objectIDOf(line);
		if (objectID > 0) {
			if (auto rule = findRule(rules, objectID)) {
				auto const target = props::parse(line);

				std::string built;
				built.reserve(rule->objects.size() * 48);

				for (auto const& object : rule->objects) {
					if (!built.empty()) {
						built += ';';
					}
					buildReplacement(object, target, settings, built);
				}

				if (!built.empty()) {
					out += built;
					expanded = true;
				}
			}
		}

		if (!expanded) {
			out.append(line);
		}

		if (semicolon == std::string::npos) {
			break;
		}

		out += ';';
		pos = semicolon + 1;
	}

	return out;
}

}
