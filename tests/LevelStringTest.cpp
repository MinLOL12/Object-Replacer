// Standalone tests for the level string rewriter.
//
//   g++ -std=c++20 -O1 -I../src LevelStringTest.cpp ../src/LevelString.cpp -o test
//
// These are not part of the mod build.

#include "LevelString.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace objrep;

namespace {

int failCount = 0;
int passCount = 0;

void check(bool condition, std::string const& what) {
	if (condition) {
		++passCount;
	}
	else {
		++failCount;
		std::printf("FAIL: %s\n", what.c_str());
	}
}

void checkEqual(std::string const& actual, std::string const& expected, std::string const& what) {
	if (actual == expected) {
		++passCount;
	}
	else {
		++failCount;
		std::printf("FAIL: %s\n   expected: %s\n   actual:   %s\n", what.c_str(), expected.c_str(), actual.c_str());
	}
}

void checkNear(double actual, double expected, std::string const& what) {
	if (std::fabs(actual - expected) < 0.01) {
		++passCount;
	}
	else {
		++failCount;
		std::printf("FAIL: %s\n   expected: %f\n   actual:   %f\n", what.c_str(), expected, actual);
	}
}

std::string valueOf(std::string const& objectString, std::string const& key) {
	auto list = props::parse(objectString);
	auto value = props::find(list, key);
	return std::string(value);
}

double numberOf(std::string const& objectString, std::string const& key) {
	return props::number(props::parse(objectString), key);
}

// --- tests -----------------------------------------------------------------

void testParseAndSerialize() {
	auto list = props::parse("1,8,2,150,3,150");
	check(list.size() == 3, "parse finds three pairs");
	checkEqual(list[0].first, "1", "first key");
	checkEqual(list[0].second, "8", "first value");
	checkEqual(props::serialize(list), "1,8,2,150,3,150", "round trip");

	// A dangling key without a value carries no information (GD ignores it too),
	// so it simply disappears.
	auto odd = props::parse("1,8,2,150,3");
	checkEqual(props::serialize(odd), "1,8,2,150", "dangling key is dropped");
}

void testFormatting() {
	checkEqual(props::formatNumber(150.0), "150", "whole number");
	checkEqual(props::formatNumber(150.5), "150.5", "half");
	checkEqual(props::formatNumber(-0.00001), "0", "negative zero");
	checkEqual(props::formatNumber(1.0 / 3.0), "0.3333", "rounding");
	checkEqual(props::formatNumber(1234.56789), "1234.5679", "four decimals");
}

void testSimpleReplacement() {
	Rule rule;
	rule.targetID = 8;
	rule.objects.push_back({ "1,1,2,0,3,0", 0.0, 0.0 });

	std::vector<Rule> rules{ rule };
	RewriteSettings settings;

	auto out = rewriteLevelString("1,8,2,150,3,75", rules, settings);
	checkEqual(out, "1,1,2,150,3,75", "single object swap keeps position");

	// nothing to do
	checkEqual(rewriteLevelString("1,9,2,150,3,75", rules, settings), "1,9,2,150,3,75", "other objects untouched");
}

void testMultiObjectOffsets() {
	Rule rule;
	rule.targetID = 8;
	// A "custom spike": a block and a decoration 30 units to the right.
	rule.objects.push_back({ "1,1,2,0,3,0", 0.0, 0.0 });
	rule.objects.push_back({ "1,211,2,0,3,0", 30.0, 0.0 });

	std::vector<Rule> rules{ rule };
	RewriteSettings settings;

	auto out = rewriteLevelString("1,8,2,300,3,90", rules, settings);

	// Split result into objects
	std::vector<std::string> objects;
	size_t pos = 0;
	while (true) {
		size_t semi = out.find(';', pos);
		objects.push_back(out.substr(pos, semi == std::string::npos ? std::string::npos : semi - pos));
		if (semi == std::string::npos) break;
		pos = semi + 1;
	}

	check(objects.size() == 2, "two objects were generated");
	checkNear(numberOf(objects[0], "2"), 300, "first object x");
	checkNear(numberOf(objects[0], "3"), 90, "first object y");
	checkNear(numberOf(objects[1], "2"), 330, "second object x offset by 30");
	checkNear(numberOf(objects[1], "3"), 90, "second object y unchanged");
	checkEqual(valueOf(objects[1], "1"), "211", "second object id");
}

void testRotationFollowsTarget() {
	Rule rule;
	rule.targetID = 8;
	rule.objects.push_back({ "1,1,2,0,3,0", 30.0, 0.0 });

	std::vector<Rule> rules{ rule };
	RewriteSettings settings;

	// Target rotated 90 degrees clockwise: the offset (30, 0) should end up
	// pointing down (y - 30) because screen rotation is clockwise.
	auto out = rewriteLevelString("1,8,2,100,3,100,6,90", rules, settings);
	checkNear(numberOf(out, "2"), 100, "rotated x");
	checkNear(numberOf(out, "3"), 70, "rotated y");
	checkNear(numberOf(out, "6"), 90, "replacement rotates with the target");
}

void testScaleFollowsTarget() {
	Rule rule;
	rule.targetID = 8;
	rule.objects.push_back({ "1,1,2,0,3,0", 30.0, 0.0 });

	std::vector<Rule> rules{ rule };
	RewriteSettings settings;

	auto out = rewriteLevelString("1,8,2,0,3,0,128,2,129,2", rules, settings);
	checkNear(numberOf(out, "2"), 60, "offset scaled by target scale x");
	checkNear(numberOf(out, "128"), 2, "replacement scale x multiplied");
	checkNear(numberOf(out, "129"), 2, "replacement scale y multiplied");

	// Turning the setting off keeps the raw offset and scale.
	settings.transform = false;
	auto raw = rewriteLevelString("1,8,2,0,3,0,128,2,129,2", rules, settings);
	checkNear(numberOf(raw, "2"), 30, "offset untouched when transform is off");
	checkEqual(valueOf(raw, "128"), "", "replacement scale untouched when transform is off");
	checkEqual(valueOf(raw, "129"), "", "replacement scale untouched when transform is off");
}

void testScaleKeys() {
	// The replacement scale is read from either key 32 or key 128/129, and is
	// always written back as 128/129 like GD itself does.
	Rule rule;
	rule.targetID = 8;
	rule.objects.push_back({ "1,1,2,0,3,0,32,0.5", 0.0, 0.0 });
	std::vector<Rule> rules{ rule };
	RewriteSettings settings;

	auto out = rewriteLevelString("1,8,2,0,3,0,128,3,129,3", rules, settings);
	checkEqual(valueOf(out, "32"), "", "uniform scale key is never written");
	checkNear(numberOf(out, "128"), 1.5, "scale read from key 32 and multiplied");
	checkNear(numberOf(out, "129"), 1.5, "scale read from key 32 and multiplied (y)");
}

void testFlipFollowsTarget() {
	Rule rule;
	rule.targetID = 8;
	rule.objects.push_back({ "1,1,2,0,3,0", 30.0, 0.0 });

	std::vector<Rule> rules{ rule };
	RewriteSettings settings;

	auto out = rewriteLevelString("1,8,2,100,3,0,4,1", rules, settings);
	checkNear(numberOf(out, "2"), 70, "flipped target mirrors the offset");
	checkEqual(valueOf(out, "4"), "1", "replacement is flipped too");
}

void testGroupsAndColors() {
	Rule rule;
	rule.targetID = 8;
	rule.objects.push_back({ "1,1,2,0,3,0,21,3,22,4", 0.0, 0.0 });

	std::vector<Rule> rules{ rule };
	RewriteSettings settings;

	auto out = rewriteLevelString("1,8,2,0,3,0,57,12.13,21,1,22,2,20,3,24,2,25,5", rules, settings);
	checkEqual(valueOf(out, "57"), "12.13", "target groups inherited");
	checkEqual(valueOf(out, "21"), "1", "base color channel inherited");
	checkEqual(valueOf(out, "22"), "2", "detail color channel inherited");
	checkEqual(valueOf(out, "20"), "3", "editor layer inherited");
	checkEqual(valueOf(out, "24"), "2", "z layer inherited");
	checkEqual(valueOf(out, "25"), "5", "z order inherited");

	// Target without groups keeps the replacement's groups.
	Rule grouped;
	grouped.targetID = 8;
	grouped.objects.push_back({ "1,1,2,0,3,0,57,99", 0.0, 0.0 });
	std::vector<Rule> groupedRules{ grouped };
	auto groupedOut = rewriteLevelString("1,8,2,0,3,0", groupedRules, settings);
	checkEqual(valueOf(groupedOut, "57"), "99", "replacement keeps its own groups when target has none");

	// Layer is dropped when the target has no layer information.
	auto noLayer = rewriteLevelString("1,8,2,0,3,0", rules, settings);
	checkEqual(valueOf(noLayer, "20"), "", "no editor layer when target has none");
	checkEqual(valueOf(noLayer, "24"), "", "no z layer when target has none");

	settings.inheritGroups = false;
	settings.inheritColor = false;
	settings.inheritLayer = false;
	auto plain = rewriteLevelString("1,8,2,0,3,0,57,12,21,1,20,3", rules, settings);
	checkEqual(valueOf(plain, "57"), "", "groups not inherited when disabled");
	checkEqual(valueOf(plain, "21"), "3", "colors not inherited when disabled");
	checkEqual(valueOf(plain, "20"), "", "layers not inherited when disabled");
}

void testSettingsHeaderIsPreserved() {
	Rule rule;
	rule.targetID = 8;
	rule.objects.push_back({ "1,1,2,0,3,0", 0.0, 0.0 });

	std::vector<Rule> rules{ rule };
	RewriteSettings settings;

	std::string level = "kA13,0,kA15,0,kA2,0;1,8,2,30,3,30;1,9,2,60,3,30;";
	auto out = rewriteLevelString(level, rules, settings);

	checkEqual(
		out,
		"kA13,0,kA15,0,kA2,0;1,1,2,30,3,30;1,9,2,60,3,30;",
		"settings header and separators preserved"
	);
}

void testDisabledRuleAndEmpty() {
	Rule rule;
	rule.targetID = 8;
	rule.enabled = false;
	rule.objects.push_back({ "1,1,2,0,3,0", 0.0, 0.0 });

	std::vector<Rule> rules{ rule };
	RewriteSettings settings;

	checkEqual(
		rewriteLevelString("1,8,2,0,3,0", rules, settings),
		"1,8,2,0,3,0",
		"disabled rule does nothing"
	);

	checkEqual(rewriteLevelString("", rules, settings), "", "empty level string");
	checkEqual(rewriteLevelString("1,8,2,0,3,0", {}, settings), "1,8,2,0,3,0", "no rules");
}

void testSelfReplacementDoesNotLoop() {
	Rule rule;
	rule.targetID = 8;
	rule.objects.push_back({ "1,8,2,0,3,0", 0.0, 0.0 });
	rule.objects.push_back({ "1,8,2,0,3,0", 0.0, 30.0 });

	std::vector<Rule> rules{ rule };
	RewriteSettings settings;

	auto out = rewriteLevelString("1,8,2,0,3,0", rules, settings);
	checkEqual(out, "1,8,2,0,3,0;1,8,2,0,3,30", "replacement of the same id is not expanded again");
}

void testBigLevel() {
	Rule rule;
	rule.targetID = 8;
	rule.objects.push_back({ "1,211,2,0,3,0", 0.0, 0.0 });

	std::vector<Rule> rules{ rule };
	RewriteSettings settings;

	std::string level;
	level.reserve(2 * 1024 * 1024);
	level += "kA13,0;";
	for (int i = 0; i < 20000; ++i) {
		level += "1,8,2," + std::to_string(i * 30) + ",3,15;";
	}

	auto out = rewriteLevelString(level, rules, settings);
	check(out.find("1,8,") == std::string::npos, "every spike was replaced");
	check(out.find("1,211,2,0,3,15") != std::string::npos, "first spike replaced correctly");
	check(std::count(out.begin(), out.end(), ';') == 20001, "object count unchanged");
}

}

int main() {
	testParseAndSerialize();
	testFormatting();
	testSimpleReplacement();
	testMultiObjectOffsets();
	testRotationFollowsTarget();
	testScaleFollowsTarget();
	testScaleKeys();
	testFlipFollowsTarget();
	testGroupsAndColors();
	testSettingsHeaderIsPreserved();
	testDisabledRuleAndEmpty();
	testSelfReplacementDoesNotLoop();
	testBigLevel();

	std::printf("\n%d passed, %d failed\n", passCount, failCount);
	return failCount == 0 ? 0 : 1;
}
