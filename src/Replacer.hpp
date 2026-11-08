#pragma once

#include <Geode/Geode.hpp>
#include <matjson.hpp>

#include <optional>
#include <string>
#include <vector>

#include "LevelString.hpp"

namespace objrep {

struct CaptureResult {
	bool ok = false;
	std::string error;
	std::vector<CapturedObject> objects;
};

class Manager {
public:
	static Manager& get();

	void load();
	void save();

	std::vector<Rule>& rules() { return m_rules; }
	std::vector<Rule> const& rules() const { return m_rules; }

	bool hasEnabledRules() const;
	Rule const* ruleForID(int objectID) const;

	void addRule(Rule rule);
	void replaceRule(size_t index, Rule rule);
	void removeRule(size_t index);
	void setRuleEnabled(size_t index, bool enabled);

	// Rewrites a level string, expanding every matching object into its
	// replacement. Returns the input untouched when there is nothing to do.
	std::string apply(std::string const& levelString) const;

private:
	std::vector<Rule> m_rules;
	bool m_loaded = false;
};

// Reads the current editor selection into a replacement. Fails with a
// user-facing message when the editor is not open or nothing is selected.
CaptureResult captureSelection();

// Object ID of the single selected editor object, or -1.
int selectedObjectID();

}
