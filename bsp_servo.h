#pragma once
#ifndef __BSP_SERVO_H__
#define __BSP_SERVO_H__

#include <Arduino.h>
#include <ESP32Servo.h>

class ServoSystem {
private:
    Servo servoLeft;
    Servo servoRight;

    // 【硬件保护机制】禁止舵机运行在 0° 或 180° 的机械极限区
    static const int MIN_SAFE_ANGLE = 15;
    static const int MAX_SAFE_ANGLE = 165;

    // 私有辅助方法，用于安全的角度映射
    int ClampAngle(int angle);

public:
    void Init(uint8_t pin_left, uint8_t pin_right);

    // 设置双臂角度接口
    void SetArms(int angleLeft, int angleRight);

    // 断开 PWM 信号，让舵机失去扭力（用于睡眠模式极致省电）
    void Detach();
    void Attach(uint8_t pin_left, uint8_t pin_right);
};

#endif