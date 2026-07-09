#pragma once
#ifndef __APP_INTERACTION_H__
#define __APP_INTERACTION_H__

#include <Arduino.h>
#include "core_sys.h"
#include "bsp_button.h"
#include "bsp_encoder.h"

class InteractionManager {
private:
    ButtonSystem head_btn;
    EncoderSystem encoder;

    // ⛔ 移除：将冷却逻辑彻底移交至主循环的动态多巴胺系统
    // uint32_t last_pet_time;
    // static const uint32_t PET_COOLDOWN_MS = 10000;
        
    // 编码器积分与超时系统
    int32_t accumulated_steps;
    uint32_t last_rotate_time;
    static const uint32_t ENCODER_TIMEOUT_MS = 2000; // 停顿超过2秒
    static const int32_t STEPS_PER_REV = 20;         // 旋转一圈的脉冲数(视具体硬件调整，通常为20或24)

public:
    void Init(uint8_t btn_pin, uint8_t enc_a_pin, uint8_t enc_b_pin);
    InteractionEvent Update(bool is_sleeping);
};

#endif