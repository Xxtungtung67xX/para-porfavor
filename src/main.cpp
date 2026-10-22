#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <deque>
#include <unordered_map>

using namespace geode::prelude;

namespace {
	struct QueuedInput {
		bool down;
		int button;
		bool isPlayer1;
		int dueTick;
	};

	std::deque<QueuedInput> s_queue;
	int s_tick = 0;
	bool s_replaying = false;   // evita re-encolar lo que nosotros mismos aplicamos
	int s_stage = 1;            // 0 = OFF, 1 = 2 ticks, 2 = 4 ticks, 3 = 8 ticks
	constexpr int kStageTicks[] = {0, 2, 4, 8};

	int currentDelay() { return kStageTicks[s_stage]; }

	// Convierte el nombre elegido en los ajustes a la tecla real
	enumKeyCodes cycleKey() {
		static const std::unordered_map<std::string, enumKeyCodes> keys = {
			{"F1", KEY_F1}, {"F2", KEY_F2}, {"F3", KEY_F3}, {"F4", KEY_F4},
			{"F5", KEY_F5}, {"F6", KEY_F6}, {"F7", KEY_F7}, {"F8", KEY_F8},
			{"F9", KEY_F9}, {"F10", KEY_F10}, {"F11", KEY_F11}, {"F12", KEY_F12},
			{"Tab", KEY_Tab}, {"Q", KEY_Q}, {"E", KEY_E}, {"R", KEY_R},
			{"T", KEY_T}, {"Z", KEY_Z}, {"X", KEY_X}, {"C", KEY_C},
			{"V", KEY_V}, {"B", KEY_B}, {"N", KEY_N}, {"M", KEY_M},
		};
		auto name = Mod::get()->getSettingValue<std::string>("cycle-key");
		auto it = keys.find(name);
		return it != keys.end() ? it->second : KEY_F6;
	}

	void resetState() {
		s_queue.clear();
		s_tick = 0;
	}
}

class $modify(PIBGameLayer, GJBaseGameLayer) {
	// Todos los press/release pasan por aqui
	void handleButton(bool down, int button, bool isPlayer1) {
		bool active = Mod::get()->getSettingValue<bool>("enabled")
			&& !s_replaying
			&& currentDelay() > 0
			&& static_cast<GJBaseGameLayer*>(PlayLayer::get()) == this;

		if (!active) {
			GJBaseGameLayer::handleButton(down, button, isPlayer1);
			return;
		}

		s_queue.push_back({down, button, isPlayer1, s_tick + currentDelay()});
	}

	// Se llama una vez por tick fisico (1 tick = 1/240 s con TPS por defecto)
	void processCommands(float dt, bool isHalfTick, bool isLastTick) {
		if (static_cast<GJBaseGameLayer*>(PlayLayer::get()) == this) {
			++s_tick;
			s_replaying = true;
			while (!s_queue.empty() && s_queue.front().dueTick <= s_tick) {
				auto q = s_queue.front();
				s_queue.pop_front();
				GJBaseGameLayer::handleButton(q.down, q.button, q.isPlayer1);
			}
			s_replaying = false;
		}
		GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
	}
};

class $modify(PIBPlayLayer, PlayLayer) {
	void resetLevel() {
		resetState();
		PlayLayer::resetLevel();
	}

	void onQuit() {
		resetState();
		PlayLayer::onQuit();
	}
};

class $modify(PIBKeyboard, CCKeyboardDispatcher) {
	bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat, double timestamp) {
		if (down && !repeat && key == cycleKey() && PlayLayer::get()) {
			s_stage = (s_stage + 1) % 4;
			resetState();
			auto msg = currentDelay() == 0
				? std::string("Input Buffer: OFF")
				: fmt::format("Input Buffer: {} ticks", currentDelay());
			Notification::create(msg, NotificationIcon::None, 0.8f)->show();
		}
		return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, timestamp);
	}
};
