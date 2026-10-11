#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

class $modify(NoclipPlayLayer, PlayLayer) {
	struct Fields {
		int totalFrames = 0;
		int deadFrames = 0;
		bool hitThisFrame = false;
		bool hitLastFrame = false;
		CCLabelBMFont* accuracyLabel = nullptr;
		CCLayerColor* flashLayer = nullptr;
	};

	bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
		if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

		auto winSize = CCDirector::get()->getWinSize();
		auto f = m_fields.self();

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

		return true;
	}

	void resetLevel() {
		PlayLayer::resetLevel();

		auto f = m_fields.self();
		f->totalFrames = 0;
		f->deadFrames = 0;
		f->hitThisFrame = false;
		f->hitLastFrame = false;
		this->refreshAccuracyLabel();
	}

	void destroyPlayer(PlayerObject* player, GameObject* object) {
		// Never swallow the anti-cheat spike, GD uses it to verify the level start
		bool isAnticheat = object == m_anticheatSpike;

		if (!isAnticheat) {
			m_fields->hitThisFrame = true;
		}

		bool noclip = Mod::get()->getSettingValue<bool>("enable-noclip");
		if (isAnticheat || !noclip) {
			PlayLayer::destroyPlayer(player, object);
		}
	}

	void postUpdate(float dt) {
		PlayLayer::postUpdate(dt);

		auto f = m_fields.self();

		// Don't count the end-of-level animation against accuracy
		if (m_hasCompletedLevel || m_levelEndAnimationStarted) {
			f->hitThisFrame = false;
			f->hitLastFrame = false;
			return;
		}

		f->totalFrames++;

		bool hit = f->hitThisFrame;
		if (hit) {
			f->deadFrames++;

			bool continuous = Mod::get()->getSettingValue<bool>("flash-continuous");
			if (continuous || !f->hitLastFrame) {
				this->triggerDeathFlash();
			}
		}

		// Updating the label every frame is wasteful; refresh periodically or on a hit
		if (hit || f->totalFrames % 8 == 0) {
			this->refreshAccuracyLabel();
		}

		f->hitLastFrame = hit;
		f->hitThisFrame = false;
	}

	void triggerDeathFlash() {
		if (!Mod::get()->getSettingValue<bool>("flash-enabled")) return;

		auto f = m_fields.self();
		if (!f->flashLayer) return;

		auto color = Mod::get()->getSettingValue<ccColor4B>("flash-color");
		auto duration = static_cast<float>(Mod::get()->getSettingValue<double>("flash-duration"));

		f->flashLayer->stopAllActions();
		f->flashLayer->setColor({ color.r, color.g, color.b });
		f->flashLayer->setOpacity(color.a);
		f->flashLayer->runAction(CCFadeTo::create(duration, 0));
	}

	void refreshAccuracyLabel() {
		auto f = m_fields.self();
		if (!f->accuracyLabel) return;

		bool show = Mod::get()->getSettingValue<bool>("show-accuracy");
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
};
