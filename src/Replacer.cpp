#include "Replacer.hpp"

#include <Geode/binding/EditorUI.hpp>
#include <Geode/binding/GJBaseGameLayer.hpp>
#include <Geode/binding/GameObject.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/utils/cocos.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <cstdlib>

using namespace geode::prelude;

namespace objrep {

namespace {

	constexpr char const* SAVE_KEY = "rules";

	RewriteSettings readSettings() {
		auto mod = Mod::get();

		RewriteSettings settings;
		settings.inheritGroups = mod->getSettingValue<bool>("inherit-groups");
		settings.inheritColor = mod->getSettingValue<bool>("inherit-color");
		settings.inheritLayer = mod->getSettingValue<bool>("inherit-layer");
		settings.transform = mod->getSettingValue<bool>("transform-with-target");
		return settings;
	}

	matjson::Value ruleToJson(Rule const& rule) {
		auto objects = matjson::Value::array();

		for (auto const& object : rule.objects) {
			auto entry = matjson::Value::object();
			entry["p"] = object.props;
			entry["dx"] = object.dx;
			entry["dy"] = object.dy;
			objects.push(entry);
		}

		auto json = matjson::Value::object();
		json["target"] = rule.targetID;
		json["enabled"] = rule.enabled;
		json["objects"] = objects;
		return json;
	}

	std::optional<Rule> ruleFromJson(matjson::Value const& json) {
		if (!json.isObject()) {
			return std::nullopt;
		}

		Rule rule;
		rule.targetID = static_cast<int>(json["target"].asInt().unwrapOr(0));
		rule.enabled = json["enabled"].asBool().unwrapOr(true);

		auto const& objects = json["objects"];
		if (!objects.isArray()) {
			return std::nullopt;
		}

		for (auto const& entry : objects) {
			CapturedObject object;
			object.props = entry["p"].asString().unwrapOr("");
			object.dx = entry["dx"].asDouble().unwrapOr(0.0);
			object.dy = entry["dy"].asDouble().unwrapOr(0.0);

			if (!object.props.empty()) {
				rule.objects.push_back(std::move(object));
			}
		}

		if (rule.targetID <= 0 || rule.objects.empty()) {
			return std::nullopt;
		}

		return rule;
	}

}

// ---------------------------------------------------------------------------
// Manager
// ---------------------------------------------------------------------------

Manager& Manager::get() {
	static Manager manager;
	return manager;
}

void Manager::load() {
	if (m_loaded) {
		return;
	}
	m_loaded = true;

	m_rules.clear();

	auto saved = Mod::get()->getSavedValue<matjson::Value>(SAVE_KEY, matjson::Value::array());
	if (saved.isArray()) {
		for (auto const& entry : saved) {
			if (auto rule = ruleFromJson(entry)) {
				m_rules.push_back(std::move(*rule));
			}
		}
	}

	log::info("Loaded {} replacement rule(s)", m_rules.size());
}

void Manager::save() {
	auto array = matjson::Value::array();
	for (auto const& rule : m_rules) {
		array.push(ruleToJson(rule));
	}

	Mod::get()->setSavedValue(SAVE_KEY, array);
	log::info("Saved {} replacement rule(s)", m_rules.size());
}

bool Manager::hasEnabledRules() const {
	if (!Mod::get()->getSettingValue<bool>("enabled")) {
		return false;
	}

	for (auto const& rule : m_rules) {
		if (rule.enabled && !rule.objects.empty()) {
			return true;
		}
	}
	return false;
}

Rule const* Manager::ruleForID(int objectID) const {
	for (auto const& rule : m_rules) {
		if (rule.enabled && rule.targetID == objectID && !rule.objects.empty()) {
			return &rule;
		}
	}
	return nullptr;
}

void Manager::addRule(Rule rule) {
	m_rules.push_back(std::move(rule));
	save();
}

void Manager::replaceRule(size_t index, Rule rule) {
	if (index >= m_rules.size()) {
		return;
	}
	m_rules[index] = std::move(rule);
	save();
}

void Manager::removeRule(size_t index) {
	if (index >= m_rules.size()) {
		return;
	}
	m_rules.erase(m_rules.begin() + static_cast<std::ptrdiff_t>(index));
	save();
}

void Manager::setRuleEnabled(size_t index, bool enabled) {
	if (index >= m_rules.size()) {
		return;
	}
	m_rules[index].enabled = enabled;
	save();
}

std::string Manager::apply(std::string const& levelString) const {
	if (levelString.empty() || !this->hasEnabledRules()) {
		return levelString;
	}

	size_t const before = levelString.size();
	auto out = rewriteLevelString(levelString, m_rules, readSettings());

	if (out.size() != before) {
		log::debug("Object Replacer: level string {} -> {} bytes", before, out.size());
	}

	return out;
}

// ---------------------------------------------------------------------------
// editor helpers
// ---------------------------------------------------------------------------

CaptureResult captureSelection() {
	CaptureResult result;

	auto ui = EditorUI::get();
	if (!ui) {
		result.error = "Open the editor first";
		return result;
	}

	auto selected = ui->getSelectedObjects();
	if (!selected || selected->count() == 0) {
		result.error = "Select the objects to use as the replacement first";
		return result;
	}

	struct Temporary {
		std::string props;
		double x;
		double y;
	};

	std::vector<Temporary> temporary;
	double minX = 0.0;
	double minY = 0.0;
	bool first = true;

	for (auto object : cocos::CCArrayExt<GameObject*>(selected)) {
		if (!object) {
			continue;
		}

		auto saved = std::string(object->getSaveString(ui->m_editorLayer));
		if (saved.empty()) {
			continue;
		}

		auto parsed = props::parse(saved);
		if (props::find(parsed, "1").empty()) {
			continue;
		}

		double const x = props::number(parsed, "2");
		double const y = props::number(parsed, "3");

		if (first) {
			minX = x;
			minY = y;
			first = false;
		}
		else {
			minX = std::min(minX, x);
			minY = std::min(minY, y);
		}

		temporary.push_back({ std::move(saved), x, y });
	}

	if (temporary.empty()) {
		result.error = "Could not read the selected objects";
		return result;
	}

	result.objects.reserve(temporary.size());
	for (auto& entry : temporary) {
		result.objects.push_back({ std::move(entry.props), entry.x - minX, entry.y - minY });
	}

	result.ok = true;
	return result;
}

int selectedObjectID() {
	auto ui = EditorUI::get();
	if (!ui) {
		return -1;
	}

	auto selected = ui->getSelectedObjects();
	if (!selected || selected->count() == 0) {
		return -1;
	}

	auto first = static_cast<GameObject*>(selected->firstObject());
	return first ? first->m_objectID : -1;
}

}
