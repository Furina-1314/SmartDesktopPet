#pragma once
#ifndef __APP_MOTION_H__
#define __APP_MOTION_H__

#include <Arduino.h>
#include "core_sys.h"
#include "bsp_servo.h"

class MotionController {
private:
    uint8_t pinL, pinR;
    uint32_t last_frame_time;
    uint8_t current_frame;

public:
    ServoSystem servos; // 保证底层接口暴露或直接在类内部操作
    PetMotion current_motion;

    void Init(uint8_t pin_l, uint8_t pin_r);
    void Update(bool is_sleeping);
    void TriggerMotion(PetMotion motion);
    void ExecuteFrame(uint32_t current_time);
    PetMotion GetCurrentMotion();

    // 【新增】强制使舵机移动至 +80° 睡眠物理姿态
    void ForceSleepPosture();

    // 注：UpdateAutonomousBehavior() 已被废弃，移至 ino 文件的主循环执行严谨定量概率
};

#endif