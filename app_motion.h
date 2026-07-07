#pragma once
#ifndef __APP_MOTION_H__
#define __APP_MOTION_H__

#include <Arduino.h>
#include "core_sys.h"
#include "bsp_servo.h"

class MotionController {
private:
    ServoSystem servos;
    uint8_t pinL;
    uint8_t pinR;

    PetMotion current_motion;
    uint8_t current_frame;
    uint32_t last_frame_time;

    // 【新增】自主行为计时器变量
    uint32_t last_idle_time;
    static const uint32_t AUTO_ACT_INTERVAL_MS = 6000; // 空闲 6 秒后触发自主行为

    void ExecuteFrame(uint32_t current_time);

public:
    void Init(uint8_t pin_left, uint8_t pin_right);
    void TriggerMotion(PetMotion new_motion);
    void Update(bool is_sleeping);

    // 【新增】自主行为评估接口
    void UpdateAutonomousBehavior(uint8_t emotion, bool is_sleeping);

    PetMotion GetCurrentMotion() const { return current_motion; }
};

#endif