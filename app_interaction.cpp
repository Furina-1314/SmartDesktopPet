#include "app_interaction.h"

void InteractionManager::Init(uint8_t btn_pin, uint8_t enc_a_pin, uint8_t enc_b_pin) {
    head_btn.Init(btn_pin);
    encoder.Init(enc_a_pin, enc_b_pin);

    last_pet_time = 0;
    accumulated_steps = 0;
    last_rotate_time = 0;
}

InteractionEvent InteractionManager::Update(bool is_sleeping) {
    uint32_t current_time = millis();

    // ==========================================
    // 1. 编码器系统判定 (喂食逻辑)
    // ==========================================
    int32_t delta = encoder.GetAndClearDelta();

    // 如果处于清醒状态且有旋转发生
    if (!is_sleeping && delta != 0) {

        // 判定超时：停顿超过 2 秒，强制清空
        if ((current_time - last_rotate_time) > ENCODER_TIMEOUT_MS) {
            accumulated_steps = 0;
            // 调试信息：让你知道是因为超时清零的
            LOG_INFO("Feed Timeout Reset");
        }

        // 【核心修改】引入机械回弹容错机制 (Backlash Tolerance)
        // 采用代数累加，不再一刀切清零
        accumulated_steps += delta;

        // 跌破 0 则截断，防止逆时针一直转导致刷出负数，保证顺时针必须从 0 算起
        if (accumulated_steps < 0) {
            accumulated_steps = 0;
        }

        last_rotate_time = current_time;

        // 【关键调试钩子】实时打印当前的积分池状态
        // 让你直接看清物理脉冲是多少，如果一直到不了 24，你就知道该改什么数字了
        Serial.printf("[%lu ms] Steps: %d / %d\n", current_time, accumulated_steps, STEPS_PER_REV);

        // 判定完成一圈
        if (accumulated_steps >= STEPS_PER_REV) {
            accumulated_steps = 0; // 触发后清零积分器
            LOG_INFO("Feed Success");
            return EVENT_FEED_SUCCESS;
        }
    }

    // ==========================================
    // 2. 按键系统判定 (摸头逻辑) 保持不变
    // ==========================================
    if (head_btn.IsPressed_Edge()) {
        if (is_sleeping) {
            LOG_INFO("Sleep Mode Off");
            return EVENT_WAKEUP;
        }
        else {
            if (current_time < last_pet_time || (current_time - last_pet_time) >= PET_COOLDOWN_MS) {
                last_pet_time = current_time;
                LOG_INFO("Head Touch");
                return EVENT_PET_SUCCESS;
            }
        }
    }

    return EVENT_NONE;
}