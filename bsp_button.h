#pragma once
#ifndef __BSP_BUTTON_H__
#define __BSP_BUTTON_H__

#include <Arduino.h>

class ButtonSystem {
private:
    uint8_t pin;
    bool last_button_state;
    bool current_button_state;
    uint32_t last_debounce_time;
    static const uint32_t DEBOUNCE_DELAY_MS = 20; // 20ms消抖时间阈值

public:
    void Init(uint8_t btn_pin);
    // 返回 true 表示检测到一次有效的“按下”边缘（下降沿）
    bool IsPressed_Edge();
};

#endif