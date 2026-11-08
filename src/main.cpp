#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "Replacer.hpp"
#include "UI.hpp"

using namespace geode::prelude;

// Every level that is played (online levels, saved levels, editor playtests)
// gets its object string rewritten before the game turns it into objects.
class $modify(ObjectReplacerPlayLayer, PlayLayer) {
public:
	$override
	void prepareCreateObjectsFromSetup(gd::string& levelString) {
		auto& manager = objrep::Manager::get();
		if (manager.hasEnabledRules()) {
			levelString = manager.apply(levelString);
		}

		PlayLayer::prepareCreateObjectsFromSetup(levelString);
	}
};

// Optional: apply the replacements while building a level as well. Saving the
// level while this is on bakes the replacements into it for good.
class $modify(ObjectReplacerLevelEditorLayer, LevelEditorLayer) {
public:
	$override
	void createObjectsFromSetup(gd::string& setup) {
		auto& manager = objrep::Manager::get();
		if (manager.hasEnabledRules() && Mod::get()->getSettingValue<bool>("apply-in-editor")) {
			setup = manager.apply(setup);
		}

		LevelEditorLayer::createObjectsFromSetup(setup);
	}
};

// A "Replace" button in the editor's top bar.
class $modify(ObjectReplacerEditorUI, EditorUI) {
public:
	$override
	bool init(LevelEditorLayer* editorLayer) {
		if (!EditorUI::init(editorLayer)) {
			return false;
		}

		auto menu = this->getChildByID("editor-buttons-menu");
		if (!menu) {
			log::warn("Could not find 'editor-buttons-menu', use the keybind instead");
			return true;
		}

		auto button = CCMenuItemSpriteExtra::create(
			ButtonSprite::create("Replace", "bigFont.fnt", "GJ_button_01.png", 0.6f),
			this,
			menu_selector(ObjectReplacerEditorUI::onOpenReplacer)
		);
		button->setID("object-replacer-button"_spr);

		menu->addChild(button);
		menu->updateLayout();

		return true;
	}

	void onOpenReplacer(CCObject*) {
		objrep::openMainPopup();
	}
};

$on_mod(Loaded) {
	objrep::Manager::get().load();

	KeybindSettingPressedEventV3(Mod::get(), "open-menu").listen(
		[](Keybind const&, bool down, bool repeat, double) {
			if (!down || repeat) {
				return;
			}

			objrep::openMainPopup();
		}
	).leak();
}
