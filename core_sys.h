#pragma once
#ifndef __CORE_SYS_H__
#define __CORE_SYS_H__

#include <Arduino.h>

// 全局状态与事件枚举
enum InteractionEvent {
    EVENT_NONE,
    EVENT_WAKEUP,
    EVENT_PET_SUCCESS,
    EVENT_FEED_SUCCESS
};

enum PetMotion {
    MOTION_NULL = 0,
    MOTION_PLAY = 1,
    MOTION_IDLE = 2,
    MOTION_TIRE = 3,
    MOTION_FOWD = 4
};

// 严谨的日志输出宏，自动附加系统开机以来的绝对毫秒数
#define LOG_INFO(msg) Serial.printf("[%lu ms] %s\n", millis(), msg)
#define LOG_MOTION(motion_id) do { \
    if ((motion_id) != 0) { \
        Serial.printf("[%lu ms] Motion+%d\n", millis(), (motion_id)); \
    } \
} while(0)
#endif // __CORE_SYS_H__
