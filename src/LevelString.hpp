#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

// This file (and LevelString.cpp) intentionally only depends on the standard
// library: it is the part of the mod that rewrites level strings, and keeping
// it free of game / Geode code makes it testable on its own.

namespace objrep {

// A single object of a replacement. The object is stored as GD's own object save
// string ("1,8,2,150,3,150,...") so the game itself can parse it back, and the
// offset is the distance from the bottom left corner of the captured selection,
// in level units.
struct CapturedObject {
	std::string props;
	double dx = 0.0;
	double dy = 0.0;
};

// "Every object with targetID becomes `objects`."
struct Rule {
	int targetID = 0;
	bool enabled = true;
	std::vector<CapturedObject> objects;
};

struct RewriteSettings {
	bool inheritGroups = true;
	bool inheritColor = true;
	bool inheritLayer = true;
	bool transform = true;
};

namespace props {
	// Ordered list of key -> value pairs of an object string.
	using List = std::vector<std::pair<std::string, std::string>>;

	List parse(std::string_view objectString);
	std::string serialize(List const& list);

	std::string_view find(List const& list, std::string_view key);
	void set(List& list, std::string_view key, std::string value);
	void erase(List& list, std::string_view key);

	double number(List const& list, std::string_view key, double fallback = 0.0);
	bool flag(List const& list, std::string_view key);

	// Scale is stored either as 128 / 129 (separate axes) or as 32 (uniform).
	double scaleX(List const& list);
	double scaleY(List const& list);
	void setScale(List& list, double scaleX, double scaleY);

	std::string formatNumber(double value);
}

// Object ids the game must not lose while the string is being rewritten.
int objectIDOf(std::string_view objectLine);

// Expands every object that matches an enabled rule. Returns a copy of the input
// when nothing matches.
std::string rewriteLevelString(
	std::string const& levelString,
	std::vector<Rule> const& rules,
	RewriteSettings const& settings
);

}
