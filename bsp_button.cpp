#include "bsp_button.h"

void ButtonSystem::Init(uint8_t btn_pin) {
    pin = btn_pin;
    pinMode(pin, INPUT_PULLUP); // 激活片内上拉电阻
    last_button_state = HIGH;   // 默认高电平
    current_button_state = HIGH;
    last_debounce_time = 0;
}

bool ButtonSystem::IsPressed_Edge() {
    bool reading = digitalRead(pin);
    bool edge_detected = false;

    // 如果电平发生任何跳变，重置消抖计时器
    if (reading != last_button_state) {
        last_debounce_time = millis();
    }

    // 只有当该电平稳定维持超过 DEBOUNCE_DELAY_MS 时，才采信该状态
    if ((millis() - last_debounce_time) > DEBOUNCE_DELAY_MS) {
        if (reading != current_button_state) {
            current_button_state = reading;
            // 检测下降沿：之前是HIGH（未按下），现在是LOW（已按下）
            if (current_button_state == LOW) {
                edge_detected = true;
            }
        }
    }
    last_button_state = reading;
    return edge_detected;
}