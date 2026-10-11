#pragma once

#include <Geode/Geode.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <vector>

using namespace geode::prelude;

namespace nc {

	// ---- settings helpers -------------------------------------------------

	inline bool getBool(char const* id) {
		return Mod::get()->getSettingValue<bool>(id);
	}
	inline void setBool(char const* id, bool value) {
		Mod::get()->setSettingValue<bool>(id, value);
	}
	inline int getInt(char const* id) {
		return static_cast<int>(Mod::get()->getSettingValue<int64_t>(id));
	}
	inline void setInt(char const* id, int value) {
		Mod::get()->setSettingValue<int64_t>(id, static_cast<int64_t>(value));
	}
	inline double getDouble(char const* id) {
		return Mod::get()->getSettingValue<double>(id);
	}
	inline void setDouble(char const* id, double value) {
		Mod::get()->setSettingValue<double>(id, value);
	}

	// ---- game speed -------------------------------------------------------

	inline void setTimeScale(float scale) {
		if (auto director = CCDirector::sharedDirector()) {
			if (auto scheduler = director->getScheduler()) {
				scheduler->setTimeScale(scale);
			}
		}
	}

	// menus, pause screen and level exit always run at normal speed
	inline void resetSpeed() {
		setTimeScale(1.f);
	}

	// speed used while actually playing a level
	inline void applySpeed() {
		setTimeScale(static_cast<float>(getDouble("game-speed")));
	}

	// ---- frame rate -------------------------------------------------------

	inline void applyFps() {
		static double s_defaultInterval = 0.0;

		auto director = CCDirector::sharedDirector();
		auto app = CCApplication::sharedApplication();
		if (!director || !app) return;

		// remember the game's own interval the first time we touch it
		if (s_defaultInterval <= 0.0) {
			s_defaultInterval = director->getAnimationInterval();
		}

		if (getBool("fps-enabled")) {
			app->setAnimationInterval(1.0 / static_cast<double>(std::max(10, getInt("fps-value"))));
		} else {
			app->setAnimationInterval(s_defaultInterval);
		}
	}

}
