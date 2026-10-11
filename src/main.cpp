#include "Common.hpp"
#include "MenuPanel.hpp"

#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/GJGameLevel.hpp>

// ---------------------------------------------------------------------------
// In-level: noclip, accuracy, death flash, FPS counter, game speed, safe mode
// ---------------------------------------------------------------------------

class $modify(NoclipPlayLayer, PlayLayer) {
	struct Fields {
		int totalFrames = 0;
		int deadFrames = 0;
		bool hitThisFrame = false;
		bool hitLastFrame = false;
		bool originalTestMode = false;
		CCLabelBMFont* accuracyLabel = nullptr;
		CCLabelBMFont* fpsLabel = nullptr;
		CCLayerColor* flashLayer = nullptr;
		int fpsFrames = 0;
		std::chrono::steady_clock::time_point fpsStart = std::chrono::steady_clock::now();
	};

	bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
		if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

		auto winSize = CCDirector::get()->getWinSize();
		auto f = m_fields.self();

		f->originalTestMode = m_isTestMode;

		// Full-screen flash overlay, invisible until a hit happens
		f->flashLayer = CCLayerColor::create({ 255, 0, 0, 0 }, winSize.width, winSize.height);
		f->flashLayer->setID("flash-layer"_spr);
		f->flashLayer->setOpacity(0);
		this->addChild(f->flashLayer, 1000);

		// Accuracy readout, top left
		f->accuracyLabel = CCLabelBMFont::create("100.00%", "bigFont.fnt");
		f->accuracyLabel->setID("accuracy-label"_spr);
		f->accuracyLabel->setScale(0.45f);
		f->accuracyLabel->setOpacity(160);
		f->accuracyLabel->setAnchorPoint({ 0.f, 1.f });
		f->accuracyLabel->setPosition({ 8.f, winSize.height - 6.f });
		f->accuracyLabel->setColor({ 80, 255, 80 });
		this->addChild(f->accuracyLabel, 999);

		// FPS counter, right under the accuracy readout
		f->fpsLabel = CCLabelBMFont::create("FPS: --", "bigFont.fnt");
		f->fpsLabel->setID("fps-label"_spr);
		f->fpsLabel->setScale(0.35f);
		f->fpsLabel->setOpacity(160);
		f->fpsLabel->setAnchorPoint({ 0.f, 1.f });
		f->fpsLabel->setPosition({ 8.f, winSize.height - 24.f });
		f->fpsLabel->setVisible(nc::getBool("show-fps"));
		this->addChild(f->fpsLabel, 999);

		if (nc::getBool("fps-enabled")) {
			nc::applyFps();
		}

		return true;
	}

	void resetLevel() {
		PlayLayer::resetLevel();

		auto f = m_fields.self();
		f->totalFrames = 0;
		f->deadFrames = 0;
		f->hitThisFrame = false;
		f->hitLastFrame = false;
		m_isTestMode = f->originalTestMode;
		this->refreshAccuracyLabel();
	}

	void destroyPlayer(PlayerObject* player, GameObject* object) {
		// Never swallow the anti-cheat spike, GD uses it to verify the level start
		bool isAnticheat = object == m_anticheatSpike;

		if (!isAnticheat) {
			m_fields->hitThisFrame = true;
		}

		bool noclip = nc::getBool("enable-noclip");
		if (isAnticheat || !noclip) {
			PlayLayer::destroyPlayer(player, object);
		}
	}

	void postUpdate(float dt) {
		PlayLayer::postUpdate(dt);

		auto f = m_fields.self();

		// Don't count the end-of-level animation against accuracy, and run it at normal speed
		if (m_hasCompletedLevel || m_levelEndAnimationStarted) {
			nc::resetSpeed();
			f->hitThisFrame = false;
			f->hitLastFrame = false;
			return;
		}

		nc::applySpeed();

		f->totalFrames++;

		bool hit = f->hitThisFrame;
		if (hit) {
			f->deadFrames++;

			bool continuous = nc::getBool("flash-continuous");
			if (continuous || !f->hitLastFrame) {
				this->triggerDeathFlash();
			}
		}

		// Updating the label every frame is wasteful; refresh periodically or on a hit
		if (hit || f->totalFrames % 8 == 0) {
			this->refreshAccuracyLabel();
		}

		this->updateFpsCounter();

		f->hitLastFrame = hit;
		f->hitThisFrame = false;
	}

	// Safe mode: finish the level as if it were a test run, so nothing is saved or submitted
	void levelComplete() {
		if (nc::getBool("safe-mode")) {
			m_isTestMode = true;
		}
		PlayLayer::levelComplete();
		nc::resetSpeed();
	}

	void resume() {
		PlayLayer::resume();
		nc::applySpeed();
	}

	void onQuit() {
		nc::resetSpeed();
		PlayLayer::onQuit();
	}

	void triggerDeathFlash() {
		if (!nc::getBool("flash-enabled")) return;

		auto f = m_fields.self();
		if (!f->flashLayer) return;

		auto color = Mod::get()->getSettingValue<ccColor4B>("flash-color");
		auto duration = static_cast<float>(nc::getDouble("flash-duration"));

		f->flashLayer->stopAllActions();
		f->flashLayer->setColor({ color.r, color.g, color.b });
		f->flashLayer->setOpacity(color.a);
		f->flashLayer->runAction(CCFadeTo::create(duration, 0));
	}

	void refreshAccuracyLabel() {
		auto f = m_fields.self();
		if (!f->accuracyLabel) return;

		bool show = nc::getBool("show-accuracy");
		f->accuracyLabel->setVisible(show);
		if (!show) return;

		float accuracy = 100.f;
		if (f->totalFrames > 0) {
			accuracy = 100.f * (1.f - static_cast<float>(f->deadFrames) / static_cast<float>(f->totalFrames));
		}

		f->accuracyLabel->setString(fmt::format("{:.2f}%", accuracy).c_str());

		if (accuracy >= 95.f) {
			f->accuracyLabel->setColor({ 80, 255, 80 });
		} else if (accuracy >= 80.f) {
			f->accuracyLabel->setColor({ 255, 220, 60 });
		} else {
			f->accuracyLabel->setColor({ 255, 80, 80 });
		}
	}

	void updateFpsCounter() {
		auto f = m_fields.self();
		if (!f->fpsLabel) return;

		bool show = nc::getBool("show-fps");
		f->fpsLabel->setVisible(show);

		// measured with the real clock so game speed doesn't skew it
		f->fpsFrames++;
		auto now = std::chrono::steady_clock::now();
		double elapsed = std::chrono::duration<double>(now - f->fpsStart).count();
		if (elapsed >= 0.5) {
			if (show) {
				f->fpsLabel->setString(fmt::format("FPS: {:.0f}", static_cast<double>(f->fpsFrames) / elapsed).c_str());
			}
			f->fpsFrames = 0;
			f->fpsStart = now;
		}
	}
};

// ---------------------------------------------------------------------------
// Pause screen: floating player menu
// ---------------------------------------------------------------------------

class $modify(NoclipPauseLayer, PauseLayer) {
	void customSetup() {
		PauseLayer::customSetup();

		// the pause screen always runs at normal speed
		nc::resetSpeed();

		if (!nc::getBool("pause-menu")) return;

		auto panel = MenuPanel::create();
		if (!panel) return;

		panel->setID("player-menu"_spr);
		this->addChild(panel, 100);
	}
};

// ---------------------------------------------------------------------------
// Main menu: apply the saved FPS setting once the game is up
// ---------------------------------------------------------------------------

class $modify(NoclipMenuLayer, MenuLayer) {
	bool init() {
		if (!MenuLayer::init()) return false;

		static bool s_done = false;
		if (!s_done) {
			s_done = true;
			nc::resetSpeed();
			if (nc::getBool("fps-enabled")) {
				nc::applyFps();
			}
		}

		return true;
	}
};

// ---------------------------------------------------------------------------
// Safe mode: don't record progress percentages
// ---------------------------------------------------------------------------

#ifndef GEODE_IS_ANDROID
class $modify(NoclipGameLevel, GJGameLevel) {
	void savePercentage(int percent, bool isPracticeMode, int clicks, int attempts, bool isChkValid) {
		if (nc::getBool("safe-mode")) return;
		GJGameLevel::savePercentage(percent, isPracticeMode, clicks, attempts, isChkValid);
	}
};
#endif
