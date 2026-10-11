#pragma once

#include "Common.hpp"

namespace nc {

	struct ToggleDef {
		char const* id;
		char const* label;
	};

	inline constexpr ToggleDef kToggles[] = {
		{ "enable-noclip", "Noclip" },
		{ "safe-mode", "Safe Mode" },
		{ "show-hitboxes", "Show Hitboxes" },
		{ "show-accuracy", "Accuracy HUD" },
		{ "flash-enabled", "Death Flash" },
		{ "show-fps", "FPS Counter" },
		{ "fps-enabled", "Custom FPS" },
	};

	inline constexpr std::array<int, 11> kFpsPresets = {
		30, 60, 75, 90, 120, 144, 165, 240, 360, 480, 1000
	};

	inline constexpr std::array<double, 11> kSpeedPresets = {
		0.1, 0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0, 5.0, 10.0
	};

	// "Show Hitboxes" lives in the game's own variables, everything else in our settings
	inline bool readToggle(char const* id) {
		if (std::strcmp(id, "show-hitboxes") == 0) {
			return GameManager::sharedState()->getGameVariable("0166");
		}
		return getBool(id);
	}

	inline void writeToggle(char const* id, bool value) {
		if (std::strcmp(id, "show-hitboxes") == 0) {
			GameManager::sharedState()->setGameVariable("0166", value);
			if (auto pl = PlayLayer::get()) {
				pl->updateDebugDraw();
			}
			return;
		}

		setBool(id, value);

		if (std::strcmp(id, "fps-enabled") == 0) {
			applyFps();
		}
	}

	template <class T, size_t N>
	inline int nearestPreset(std::array<T, N> const& presets, T value) {
		int best = 0;
		double bestDiff = 1e300;
		for (size_t i = 0; i < N; ++i) {
			double diff = std::abs(static_cast<double>(presets[i]) - static_cast<double>(value));
			if (diff < bestDiff) {
				bestDiff = diff;
				best = static_cast<int>(i);
			}
		}
		return best;
	}

}

// Floating, draggable, minimizable panel shown on the pause screen.
// It is a CCMenu so buttons, toggles and touch priorities behave like the rest of the pause UI.
class MenuPanel : public CCMenu {
protected:
	struct Row {
		CCLabelBMFont* label = nullptr;
		CCNode* a = nullptr;
		CCNode* b = nullptr;
		float height = 0.f;
		bool stepper = false;
	};

	static constexpr float kWidth = 150.f;
	static constexpr float kHeader = 24.f;
	static constexpr float kToggleRow = 20.f;
	static constexpr float kStepRow = 24.f;
	static constexpr float kPad = 6.f;

	CCScale9Sprite* m_bg = nullptr;
	CCLabelBMFont* m_title = nullptr;
	ButtonSprite* m_minSpr = nullptr;
	CCMenuItemSpriteExtra* m_minItem = nullptr;
	CCLabelBMFont* m_fpsLabel = nullptr;
	CCLabelBMFont* m_speedLabel = nullptr;
	std::vector<Row> m_rows;
	float m_fullHeight = 0.f;
	bool m_minimized = false;
	bool m_dragging = false;
	bool m_swallowing = false;
	CCPoint m_lastTouch = { 0.f, 0.f };

public:
	static MenuPanel* create() {
		auto ret = new MenuPanel();
		if (ret && ret->init()) {
			ret->autorelease();
			return ret;
		}
		CC_SAFE_DELETE(ret);
		return nullptr;
	}

	bool init() override {
		if (!CCMenu::init()) return false;

		// sit just above the pause buttons so touches on the panel never hit them
		this->setTouchPriority(-150);

		this->ignoreAnchorPointForPosition(false);
		this->setAnchorPoint({ 0.f, 1.f });

		m_bg = CCScale9Sprite::create("square02b_001.png", { 0.f, 0.f, 80.f, 80.f });
		m_bg->setColor({ 0, 0, 0 });
		m_bg->setOpacity(165);
		this->addChild(m_bg, -1);

		m_title = CCLabelBMFont::create("Player Menu", "goldFont.fnt");
		m_title->setScale(0.45f);
		m_title->setAnchorPoint({ 0.f, 0.5f });
		this->addChild(m_title);

		m_minSpr = ButtonSprite::create("-", 22, true, "bigFont.fnt", "GJ_button_01.png", 18.f, 0.5f);
		m_minItem = CCMenuItemSpriteExtra::create(m_minSpr, nullptr, this, menu_selector(MenuPanel::onMinimize));
		this->addChild(m_minItem);

		// toggle rows
		for (size_t i = 0; i < std::size(nc::kToggles); ++i) {
			auto const& def = nc::kToggles[i];

			Row row;
			row.height = kToggleRow;

			row.label = CCLabelBMFont::create(def.label, "bigFont.fnt");
			row.label->setScale(0.32f);
			row.label->setAnchorPoint({ 0.f, 0.5f });
			this->addChild(row.label);

			auto toggler = CCMenuItemToggler::createWithStandardSprites(
				this, menu_selector(MenuPanel::onToggle), 0.5f
			);
			toggler->setTag(static_cast<int>(i));
			toggler->toggle(nc::readToggle(def.id));
			this->addChild(toggler);
			row.a = toggler;

			m_rows.push_back(row);
		}

		// stepper rows (label, minus button, plus button)
		auto addStepper = [this](CCLabelBMFont*& labelOut, SEL_MenuHandler callback) {
			Row row;
			row.height = kStepRow;
			row.stepper = true;

			labelOut = CCLabelBMFont::create("", "bigFont.fnt");
			labelOut->setScale(0.32f);
			labelOut->setAnchorPoint({ 0.f, 0.5f });
			this->addChild(labelOut);
			row.label = labelOut;

			auto makeButton = [this, callback](char const* text, int direction) {
				auto spr = ButtonSprite::create(text, 18, true, "bigFont.fnt", "GJ_button_01.png", 18.f, 0.5f);
				auto item = CCMenuItemSpriteExtra::create(spr, nullptr, this, callback);
				item->setTag(direction);
				this->addChild(item);
				return item;
			};
			row.a = makeButton("-", -1);
			row.b = makeButton("+", 1);

			m_rows.push_back(row);
		};

		addStepper(m_fpsLabel, menu_selector(MenuPanel::onFpsStep));
		addStepper(m_speedLabel, menu_selector(MenuPanel::onSpeedStep));

		m_fullHeight = kHeader + kPad * 2.f;
		for (auto const& row : m_rows) {
			m_fullHeight += row.height;
		}

		this->refreshLabels();

		auto win = CCDirector::sharedDirector()->getWinSize();
		m_minimized = Mod::get()->getSavedValue<bool>("menu-minimized", false);
		float x = Mod::get()->getSavedValue<float>("menu-x", 8.f);
		float y = Mod::get()->getSavedValue<float>("menu-y", win.height - 8.f);

		this->layout();
		this->setPosition(this->clampPos({ x, y }));

		return true;
	}

protected:
	CCPoint clampPos(CCPoint p) {
		auto win = CCDirector::sharedDirector()->getWinSize();
		auto size = this->getContentSize();
		p.x = std::clamp(p.x, 0.f, std::max(0.f, win.width - size.width));
		p.y = std::clamp(p.y, std::min(size.height, win.height), win.height);
		return p;
	}

	void layout() {
		float h = m_minimized ? kHeader : m_fullHeight;

		this->setContentSize({ kWidth, h });

		m_bg->setContentSize({ kWidth, h });
		m_bg->setPosition({ kWidth / 2.f, h / 2.f });

		m_title->setPosition({ 8.f, h - kHeader / 2.f });
		m_minItem->setPosition({ kWidth - 16.f, h - kHeader / 2.f });
		m_minSpr->setString(m_minimized ? "+" : "-");

		float y = m_fullHeight - kHeader - kPad;
		// rows are laid out from the top of the *full* panel; when minimized they are hidden anyway
		float offset = h - m_fullHeight;
		for (auto& row : m_rows) {
			float cy = y - row.height / 2.f + offset;
			if (row.stepper) {
				row.label->setPosition({ 10.f, cy });
				row.a->setPosition({ kWidth - 34.f, cy });
				row.b->setPosition({ kWidth - 14.f, cy });
			} else {
				row.a->setPosition({ 14.f, cy });
				row.label->setPosition({ 28.f, cy });
			}

			bool visible = !m_minimized;
			row.label->setVisible(visible);
			row.a->setVisible(visible);
			if (row.b) row.b->setVisible(visible);

			y -= row.height;
		}
	}

	void refreshLabels() {
		m_fpsLabel->setString(fmt::format("FPS: {}", nc::getInt("fps-value")).c_str());
		m_speedLabel->setString(fmt::format("Speed: {:.2f}x", nc::getDouble("game-speed")).c_str());
	}

	void savePosition() {
		auto pos = this->getPosition();
		Mod::get()->setSavedValue<float>("menu-x", pos.x);
		Mod::get()->setSavedValue<float>("menu-y", pos.y);
	}

	// ---- touch: dragging by the header, swallowing taps on the panel body ----

	bool ccTouchBegan(CCTouch* touch, CCEvent* event) override {
		m_dragging = false;
		m_swallowing = false;

		if (!this->isVisible()) return false;

		// buttons and toggles get first pick
		if (CCMenu::ccTouchBegan(touch, event)) return true;

		auto loc = touch->getLocation();
		auto local = this->convertToNodeSpace(loc);
		auto size = this->getContentSize();

		if (CCRect(0.f, size.height - kHeader, size.width, kHeader).containsPoint(local)) {
			m_dragging = true;
			m_lastTouch = loc;
			return true;
		}

		// keep taps on the empty part of the panel from reaching pause buttons underneath
		if (CCRect(0.f, 0.f, size.width, size.height).containsPoint(local)) {
			m_swallowing = true;
			return true;
		}

		return false;
	}

	void ccTouchMoved(CCTouch* touch, CCEvent* event) override {
		if (m_dragging) {
			auto loc = touch->getLocation();
			auto delta = loc - m_lastTouch;
			m_lastTouch = loc;
			this->setPosition(this->clampPos(this->getPosition() + delta));
			return;
		}
		if (m_swallowing) return;
		CCMenu::ccTouchMoved(touch, event);
	}

	void ccTouchEnded(CCTouch* touch, CCEvent* event) override {
		if (m_dragging) {
			m_dragging = false;
			this->savePosition();
			return;
		}
		if (m_swallowing) {
			m_swallowing = false;
			return;
		}
		CCMenu::ccTouchEnded(touch, event);
	}

	void ccTouchCancelled(CCTouch* touch, CCEvent* event) override {
		if (m_dragging) {
			m_dragging = false;
			this->savePosition();
			return;
		}
		if (m_swallowing) {
			m_swallowing = false;
			return;
		}
		CCMenu::ccTouchCancelled(touch, event);
	}

	// ---- callbacks ----------------------------------------------------------

	void onMinimize(CCObject*) {
		m_minimized = !m_minimized;
		Mod::get()->setSavedValue<bool>("menu-minimized", m_minimized);
		this->layout();
		this->setPosition(this->clampPos(this->getPosition()));
	}

	void onToggle(CCObject* sender) {
		auto index = static_cast<size_t>(static_cast<CCNode*>(sender)->getTag());
		if (index >= std::size(nc::kToggles)) return;

		auto const& def = nc::kToggles[index];
		nc::writeToggle(def.id, !nc::readToggle(def.id));
	}

	void onFpsStep(CCObject* sender) {
		int direction = static_cast<CCNode*>(sender)->getTag();
		int index = nc::nearestPreset(nc::kFpsPresets, nc::getInt("fps-value")) + direction;
		index = std::clamp(index, 0, static_cast<int>(nc::kFpsPresets.size()) - 1);

		nc::setInt("fps-value", nc::kFpsPresets[index]);
		if (nc::getBool("fps-enabled")) {
			nc::applyFps();
		}
		this->refreshLabels();
	}

	void onSpeedStep(CCObject* sender) {
		int direction = static_cast<CCNode*>(sender)->getTag();
		int index = nc::nearestPreset(nc::kSpeedPresets, nc::getDouble("game-speed")) + direction;
		index = std::clamp(index, 0, static_cast<int>(nc::kSpeedPresets.size()) - 1);

		// applied when the level resumes; the pause screen itself always runs at normal speed
		nc::setDouble("game-speed", nc::kSpeedPresets[index]);
		this->refreshLabels();
	}
};
