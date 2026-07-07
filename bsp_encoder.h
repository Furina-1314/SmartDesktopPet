#pragma once
#ifndef __BSP_ENCODER_H__
#define __BSP_ENCODER_H__

#include <Arduino.h>

class EncoderSystem {
private:
    uint8_t pinA;
    uint8_t pinB;

public:
    void Init(uint8_t pin_a, uint8_t pin_b);

    // 获取真实的物理“咔哒”步数
    int32_t GetAndClearDelta();
};

#endif