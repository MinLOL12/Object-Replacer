#include "UI.hpp"

#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemToggler.hpp>
#include <Geode/binding/FLAlertLayer.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <cstdlib>

using namespace geode::prelude;

namespace objrep {

namespace {

	constexpr float kPopupWidth = 400.f;
	constexpr float kMainPopupHeight = 260.f;
	constexpr float kEditPopupHeight = 270.f;

	constexpr float kRowHeight = 32.f;
	constexpr float kFirstRowY = 76.f;

	CCLabelBMFont* makeLabel(std::string const& text, float scale, ccColor3B color = ccWHITE) {
		auto label = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
		label->setScale(scale);
		label->setColor(color);
		return label;
	}

	CCMenuItemSpriteExtra* makeTextButton(
		char const* text, float scale, CCObject* target, SEL_MenuHandler selector,
		char const* background = "GJ_button_01.png"
	) {
		auto sprite = ButtonSprite::create(text, "bigFont.fnt", background, scale);
		return CCMenuItemSpriteExtra::create(sprite, target, selector);
	}

	CCMenuItemSpriteExtra* makeSmallButton(
		char const* text, CCObject* target, SEL_MenuHandler selector,
		char const* background = "GJ_button_01.png"
	) {
		return makeTextButton(text, 0.45f, target, selector, background);
	}

	void notify(std::string const& message, NotificationIcon icon = NotificationIcon::Warning) {
		Notification::create(message, icon)->show();
	}

}

// ---------------------------------------------------------------------------
// main popup
// ---------------------------------------------------------------------------

ObjectReplacerPopup* ObjectReplacerPopup::create() {
	auto ret = new ObjectReplacerPopup();
	if (ret && ret->setup()) {
		ret->autorelease();
		return ret;
	}
	CC_SAFE_DELETE(ret);
	return nullptr;
}

bool ObjectReplacerPopup::setup() {
	if (!Popup::init(kPopupWidth, kMainPopupHeight)) {
		return false;
	}

	this->setTitle("Object Replacer");
	m_noElasticity = true;

	m_rowsMenu = CCMenu::create();
	m_rowsMenu->setContentSize({ kPopupWidth, 170.f });
	m_mainLayer->addChildAtPosition(m_rowsMenu, Anchor::Center, ccp(0.f, 12.f));

	m_emptyLabel = makeLabel("No replacements yet.\nPress New Rule to create one.", 0.45f, ccc3(200, 200, 200));
	m_emptyLabel->setAlignment(kCCTextAlignmentCenter);
	m_mainLayer->addChildAtPosition(m_emptyLabel, Anchor::Center, ccp(0.f, 30.f));

	auto pageMenu = CCMenu::create();
	m_prevButton = makeSmallButton("<", this, menu_selector(ObjectReplacerPopup::onPreviousPage), "GJ_button_02.png");
	m_prevButton->setPosition(ccp(-60.f, 0.f));
	pageMenu->addChild(m_prevButton);

	m_nextButton = makeSmallButton(">", this, menu_selector(ObjectReplacerPopup::onNextPage), "GJ_button_02.png");
	m_nextButton->setPosition(ccp(60.f, 0.f));
	pageMenu->addChild(m_nextButton);

	m_mainLayer->addChildAtPosition(pageMenu, Anchor::Center, ccp(0.f, -86.f));

	m_pageLabel = makeLabel("1 / 1", 0.45f, ccc3(200, 200, 200));
	m_mainLayer->addChildAtPosition(m_pageLabel, Anchor::Center, ccp(0.f, -86.f));

	auto bottomMenu = CCMenu::create();
	bottomMenu->addChild(makeTextButton("New Rule", 0.7f, this, menu_selector(ObjectReplacerPopup::onNewRule)));
	bottomMenu->setLayout(RowLayout::create()->setGap(12.f));
	m_mainLayer->addChildAtPosition(bottomMenu, Anchor::Bottom, ccp(0.f, 24.f));

	this->refresh();
	return true;
}

void ObjectReplacerPopup::refresh() {
	auto& rules = Manager::get().rules();

	int const totalPages = std::max<int>(1, static_cast<int>((rules.size() + kRowsPerPage - 1) / kRowsPerPage));
	m_page = std::clamp(m_page, 0, totalPages - 1);

	m_rowsMenu->removeAllChildren();
	m_emptyLabel->setVisible(rules.empty());

	for (int row = 0; row < kRowsPerPage; ++row) {
		size_t const index = static_cast<size_t>(m_page) * kRowsPerPage + row;
		if (index >= rules.size()) {
			break;
		}

		auto const& rule = rules[index];
		float const y = kFirstRowY - row * kRowHeight;

		auto toggle = CCMenuItemToggler::createWithStandardSprites(
			this, menu_selector(ObjectReplacerPopup::onToggleRule), 0.7f
		);
		toggle->toggle(rule.enabled);
		toggle->setTag(static_cast<int>(index));
		toggle->setPosition(ccp(-168.f, y));
		m_rowsMenu->addChild(toggle);

		auto text = rule.objects.size() == 1
			? fmt::format("ID {}  ->  {} object", rule.targetID, rule.objects.size())
			: fmt::format("ID {}  ->  {} objects", rule.targetID, rule.objects.size());

		auto label = makeLabel(text, 0.5f, rule.enabled ? ccWHITE : ccc3(140, 140, 140));
		label->setAnchorPoint(ccp(0.f, 0.5f));
		label->setPosition(ccp(-140.f, y));
		m_rowsMenu->addChild(label);

		auto edit = makeSmallButton("Edit", this, menu_selector(ObjectReplacerPopup::onEditRule), "GJ_button_02.png");
		edit->setTag(static_cast<int>(index));
		edit->setPosition(ccp(118.f, y));
		m_rowsMenu->addChild(edit);

		auto remove = makeSmallButton("X", this, menu_selector(ObjectReplacerPopup::onDeleteRule), "GJ_button_03.png");
		remove->setTag(static_cast<int>(index));
		remove->setPosition(ccp(165.f, y));
		m_rowsMenu->addChild(remove);
	}

	bool const multiplePages = totalPages > 1;
	m_prevButton->setVisible(multiplePages);
	m_nextButton->setVisible(multiplePages);
	m_pageLabel->setVisible(multiplePages);
	m_pageLabel->setString(fmt::format("{} / {}", m_page + 1, totalPages).c_str());
}

void ObjectReplacerPopup::onNewRule(CCObject*) {
	auto popup = RuleEditPopup::create(-1, [this] {
		this->refresh();
	});

	if (popup) {
		popup->show();
	}
}

void ObjectReplacerPopup::onEditRule(CCObject* sender) {
	if (!sender) {
		return;
	}

	auto popup = RuleEditPopup::create(sender->getTag(), [this] {
		this->refresh();
	});

	if (popup) {
		popup->show();
	}
}

void ObjectReplacerPopup::onToggleRule(CCObject* sender) {
	auto toggle = static_cast<CCMenuItemToggler*>(sender);
	if (!toggle) {
		return;
	}

	Manager::get().setRuleEnabled(static_cast<size_t>(toggle->getTag()), toggle->isToggled());
	this->refresh();
}

void ObjectReplacerPopup::onDeleteRule(CCObject* sender) {
	if (!sender) {
		return;
	}

	size_t const index = static_cast<size_t>(sender->getTag());
	auto& rules = Manager::get().rules();
	if (index >= rules.size()) {
		return;
	}

	int const targetID = rules[index].targetID;

	createQuickPopup(
		"Delete Replacement",
		fmt::format("Remove the replacement for object <cy>{}</c>?", targetID),
		"Cancel", "Delete",
		[this, index](FLAlertLayer*, bool confirmed) {
			if (!confirmed) {
				return;
			}
			Manager::get().removeRule(index);
			this->refresh();
		}
	);
}

void ObjectReplacerPopup::onPreviousPage(CCObject*) {
	if (m_page > 0) {
		m_page -= 1;
		this->refresh();
	}
}

void ObjectReplacerPopup::onNextPage(CCObject*) {
	auto& rules = Manager::get().rules();
	int const totalPages = std::max<int>(1, static_cast<int>((rules.size() + kRowsPerPage - 1) / kRowsPerPage));
	if (m_page + 1 < totalPages) {
		m_page += 1;
		this->refresh();
	}
}

// ---------------------------------------------------------------------------
// rule editor
// ---------------------------------------------------------------------------

RuleEditPopup* RuleEditPopup::create(int ruleIndex, geode::Function<void()> onSaved) {
	auto ret = new RuleEditPopup();
	if (ret && ret->setup(ruleIndex, std::move(onSaved))) {
		ret->autorelease();
		return ret;
	}
	CC_SAFE_DELETE(ret);
	return nullptr;
}

bool RuleEditPopup::setup(int ruleIndex, geode::Function<void()> onSaved) {
	if (!Popup::init(kPopupWidth, kEditPopupHeight)) {
		return false;
	}

	m_ruleIndex = ruleIndex;
	m_onSaved = std::move(onSaved);

	if (m_ruleIndex >= 0) {
		auto& rules = Manager::get().rules();
		if (m_ruleIndex < static_cast<int>(rules.size())) {
			m_targetID = rules[m_ruleIndex].targetID;
			m_objects = rules[m_ruleIndex].objects;
		}
		else {
			m_ruleIndex = -1;
		}
	}

	this->setTitle(m_ruleIndex >= 0 ? "Edit Replacement" : "New Replacement");
	m_noElasticity = true;

	auto menu = CCMenu::create();
	m_mainLayer->addChildAtPosition(menu, Anchor::Center, ccp(0.f, 0.f));

	// --- target -------------------------------------------------------------
	auto targetLabel = makeLabel("Target object ID", 0.5f);
	m_mainLayer->addChildAtPosition(targetLabel, Anchor::Center, ccp(-100.f, 74.f));

	m_targetInput = TextInput::create(110.f, "e.g. 8", "bigFont.fnt");
	m_targetInput->setCommonFilter(CommonFilter::Uint);
	m_targetInput->setMaxCharCount(6);
	if (m_targetID > 0) {
		m_targetInput->setString(std::to_string(m_targetID));
	}
	m_mainLayer->addChildAtPosition(m_targetInput, Anchor::Center, ccp(-100.f, 46.f));

	auto fromSelection = makeTextButton(
		"From Selection", 0.5f, this, menu_selector(RuleEditPopup::onUseSelectedTarget), "GJ_button_02.png"
	);
	fromSelection->setPosition(ccp(72.f, 46.f));
	menu->addChild(fromSelection);

	// --- replacement --------------------------------------------------------
	auto replacementTitle = makeLabel("Replacement", 0.5f);
	m_mainLayer->addChildAtPosition(replacementTitle, Anchor::Center, ccp(0.f, 12.f));

	m_replacementLabel = makeLabel("", 0.4f, ccc3(160, 255, 160));
	m_mainLayer->addChildAtPosition(m_replacementLabel, Anchor::Center, ccp(0.f, -8.f));

	auto capture = makeTextButton(
		"Capture Selection", 0.7f, this, menu_selector(RuleEditPopup::onCaptureSelection)
	);
	capture->setPosition(ccp(0.f, -38.f));
	menu->addChild(capture);

	auto hint = makeLabel(
		"1. Select the objects you want in the editor\n"
		"2. Press Capture Selection\n"
		"3. Type the ID of the object to replace",
		0.32f, ccc3(190, 190, 190)
	);
	hint->setAlignment(kCCTextAlignmentCenter);
	m_mainLayer->addChildAtPosition(hint, Anchor::Center, ccp(0.f, -76.f));

	// --- buttons ------------------------------------------------------------
	auto buttons = CCMenu::create();
	buttons->addChild(makeTextButton("Cancel", 0.7f, this, menu_selector(RuleEditPopup::onCancel), "GJ_button_02.png"));
	buttons->addChild(makeTextButton("Save", 0.7f, this, menu_selector(RuleEditPopup::onSave)));
	buttons->setLayout(RowLayout::create()->setGap(14.f));
	m_mainLayer->addChildAtPosition(buttons, Anchor::Bottom, ccp(0.f, 24.f));

	this->updateReplacementLabel();
	return true;
}

void RuleEditPopup::updateReplacementLabel() {
	std::string text;
	if (m_objects.empty()) {
		text = "No objects captured yet";
	}
	else if (m_objects.size() == 1) {
		text = "1 object captured";
	}
	else {
		text = fmt::format("{} objects captured", m_objects.size());
	}

	m_replacementLabel->setString(text.c_str());
}

void RuleEditPopup::onUseSelectedTarget(CCObject*) {
	int const objectID = selectedObjectID();
	if (objectID <= 0) {
		notify("Select a single object in the editor first");
		return;
	}

	m_targetInput->setString(std::to_string(objectID));
}

void RuleEditPopup::onCaptureSelection(CCObject*) {
	auto result = captureSelection();

	if (!result.ok) {
		notify(result.error);
		return;
	}

	m_objects = std::move(result.objects);
	this->updateReplacementLabel();
	notify(fmt::format("Captured {} object(s)", m_objects.size()), NotificationIcon::Success);
}

void RuleEditPopup::onSave(CCObject*) {
	int const objectID = std::atoi(m_targetInput->getString().c_str());

	if (objectID <= 0) {
		notify("Enter a valid object ID first");
		return;
	}

	if (m_objects.empty()) {
		notify("Capture a replacement selection first");
		return;
	}

	auto& manager = Manager::get();
	auto& rules = manager.rules();

	Rule rule;
	rule.targetID = objectID;
	rule.enabled = true;
	rule.objects = m_objects;

	if (m_ruleIndex >= 0) {
		if (m_ruleIndex < static_cast<int>(rules.size())) {
			rule.enabled = rules[m_ruleIndex].enabled;
		}
		manager.replaceRule(static_cast<size_t>(m_ruleIndex), rule);
	}
	else {
		for (auto const& existing : rules) {
			if (existing.targetID == objectID) {
				notify(fmt::format("A replacement for object {} already exists, edit that one instead", objectID));
				return;
			}
		}
		manager.addRule(rule);
	}

	if (m_onSaved) {
		m_onSaved();
	}

	notify("Replacement saved", NotificationIcon::Success);
	this->onClose(nullptr);
}

void RuleEditPopup::onCancel(CCObject*) {
	this->onClose(nullptr);
}

void openMainPopup() {
	if (auto popup = ObjectReplacerPopup::create()) {
		popup->show();
	}
	else {
		notify("Could not open Object Replacer", NotificationIcon::Error);
	}
}

}
