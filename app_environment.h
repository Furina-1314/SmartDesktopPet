#pragma once
#ifndef __APP_ENVIRONMENT_H__
#define __APP_ENVIRONMENT_H__

#include <Arduino.h>

class EnvironmentSystem {
private:
    uint8_t do_pin;
    uint32_t dark_start_time;
    bool is_dark_timing;

public:
    EnvironmentSystem();

    // 初始化引脚，配置上拉电阻
    void Init(uint8_t pin);

    // 轮询环境状态，返回 true 表示满足连续5秒暗光条件 (触发基础功能的浅度睡眠)
    bool CheckSleepCondition(bool current_sleep_state);
};

#endif // __APP_ENVIRONMENT_H__