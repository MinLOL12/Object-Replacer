#pragma once

#include <Geode/Geode.hpp>

#include "Replacer.hpp"

namespace objrep {

// The main menu: lists every replacement and lets the user add / edit / remove
// them.
class ObjectReplacerPopup : public geode::Popup {
public:
	static ObjectReplacerPopup* create();

protected:
	static constexpr int kRowsPerPage = 5;

	int m_page = 0;
	cocos2d::CCMenu* m_rowsMenu = nullptr;
	cocos2d::CCLabelBMFont* m_pageLabel = nullptr;
	cocos2d::CCLabelBMFont* m_emptyLabel = nullptr;
	cocos2d::CCMenuItemSpriteExtra* m_prevButton = nullptr;
	cocos2d::CCMenuItemSpriteExtra* m_nextButton = nullptr;

	bool setup();
	void refresh();

	void onNewRule(cocos2d::CCObject* sender);
	void onEditRule(cocos2d::CCObject* sender);
	void onToggleRule(cocos2d::CCObject* sender);
	void onDeleteRule(cocos2d::CCObject* sender);
	void onPreviousPage(cocos2d::CCObject* sender);
	void onNextPage(cocos2d::CCObject* sender);
};

// Editor for a single replacement. `ruleIndex` is -1 when creating a new rule.
class RuleEditPopup : public geode::Popup {
public:
	static RuleEditPopup* create(int ruleIndex, geode::Function<void()> onSaved);

protected:
	int m_ruleIndex = -1;
	int m_targetID = 0;
	std::vector<CapturedObject> m_objects;
	geode::TextInput* m_targetInput = nullptr;
	cocos2d::CCLabelBMFont* m_replacementLabel = nullptr;
	geode::Function<void()> m_onSaved;

	bool setup(int ruleIndex, geode::Function<void()> onSaved);
	void updateReplacementLabel();

	void onUseSelectedTarget(cocos2d::CCObject* sender);
	void onCaptureSelection(cocos2d::CCObject* sender);
	void onSave(cocos2d::CCObject* sender);
	void onCancel(cocos2d::CCObject* sender);
};

// Opens the main menu, showing a notification if it could not be created.
void openMainPopup();

}
