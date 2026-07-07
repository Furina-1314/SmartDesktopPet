#include "bsp_servo.h"

int ServoSystem::ClampAngle(int angle) {
    if (angle < MIN_SAFE_ANGLE) return MIN_SAFE_ANGLE;
    if (angle > MAX_SAFE_ANGLE) return MAX_SAFE_ANGLE;
    return angle;
}

void ServoSystem::Init(uint8_t pin_left, uint8_t pin_right) {
    // 严谨的定时器分配：ESP32Servo 库默认自动分配可用通道
    // 设置符合工业标准的 PWM 脉宽范围 (通常为 500us 到 2500us 对应 0-180度)
    servoLeft.setPeriodHertz(50);
    servoRight.setPeriodHertz(50);

    Attach(pin_left, pin_right);

    // 初始化时置于 90° 居中位置
    SetArms(90, 90);
}

void ServoSystem::Attach(uint8_t pin_left, uint8_t pin_right) {
    if (!servoLeft.attached()) servoLeft.attach(pin_left, 500, 2500);
    if (!servoRight.attached()) servoRight.attach(pin_right, 500, 2500);
}

void ServoSystem::Detach() {
    servoLeft.detach();
    servoRight.detach();
}

void ServoSystem::SetArms(int angleLeft, int angleRight) {
    // 经过软限幅保护后再写入寄存器，彻底告别堵转烧毁
    servoLeft.write(ClampAngle(angleLeft));
    servoRight.write(ClampAngle(angleRight));
}