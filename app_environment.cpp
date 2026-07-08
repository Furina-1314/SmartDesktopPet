#include "app_environment.h"

EnvironmentSystem::EnvironmentSystem() {
    do_pin = 0;
    dark_start_time = 0;
    is_dark_timing = false;
}

void EnvironmentSystem::Init(uint8_t pin) {
    do_pin = pin;
    pinMode(do_pin, INPUT_PULLUP);
}

bool EnvironmentSystem::CheckSleepCondition(bool current_sleep_state) {
    if (current_sleep_state) {
        is_dark_timing = false;
        return false;
    }

    // 注意物理标定：假设暗光环境下 DO 输出高电平 (HIGH)
    bool is_dark_now = (digitalRead(do_pin) == HIGH);

    if (is_dark_now) {
        if (!is_dark_timing) {
            is_dark_timing = true;
            dark_start_time = millis();
        }
        else if (millis() - dark_start_time >= 5000) {
            is_dark_timing = false;
            return true;
        }
    }
    else {
        is_dark_timing = false;
    }

    return false;
}