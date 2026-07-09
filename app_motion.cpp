#include "app_motion.h"

void MotionController::Init(uint8_t pin_l, uint8_t pin_r) {
    pinL = pin_l;
    pinR = pin_r;
    servos.Init(pinL, pinR);
    current_motion = MOTION_NULL;
    current_frame = 0;
    last_frame_time = millis();
}

PetMotion MotionController::GetCurrentMotion() {
    return current_motion;
}

void MotionController::TriggerMotion(PetMotion motion) {
    current_motion = motion;
    current_frame = 0;
    last_frame_time = millis();
}

// 【新增核心实现】绝对映射控制律
void MotionController::ForceSleepPosture() {
    servos.Attach(pinL, pinR);
    servos.SetArms(170, 170); // 定量映射：90(中立) + 80(偏移) = 170度
}

void MotionController::Update(bool is_sleeping) {
    // 剔除旧版假睡眠逻辑，专注于在唤醒态分配时间切片
    if (!is_sleeping) {
        servos.Attach(pinL, pinR);
        ExecuteFrame(millis());
    }
}

void MotionController::ExecuteFrame(uint32_t current_time) {
    // 保持上一版修改过的半周期 (Half-period) 时间窗离散化逻辑不变
    // (MOTION_PLAY, MOTION_IDLE, MOTION_TIRE, MOTION_FOWD)
    switch (current_motion) {
    case MOTION_NULL:
        if (current_frame == 0) {
            servos.SetArms(90, 90);
            current_frame++;
        }
        break;

    case MOTION_PLAY: // 3次，幅度-45°~+45°，持续2秒 (333ms/帧)
        if (current_time - last_frame_time >= 333) {
            last_frame_time = current_time;
            if (current_frame % 2 == 0) servos.SetArms(45, 45);
            else servos.SetArms(135, 135);
            current_frame++;
            if (current_frame >= 6) TriggerMotion(MOTION_NULL);
        }
        break;

    case MOTION_IDLE: // 3次，幅度-60°~0°，持续4秒 (666ms/帧)
        if (current_time - last_frame_time >= 666) {
            last_frame_time = current_time;
            if (current_frame % 2 == 0) servos.SetArms(30, 30);
            else servos.SetArms(90, 90);
            current_frame++;
            if (current_frame >= 6) TriggerMotion(MOTION_NULL);
        }
        break;

    case MOTION_TIRE: // 2次，一侧静止，另一侧30°~50°，持续3秒 (750ms/帧)
        if (current_time - last_frame_time >= 750) {
            last_frame_time = current_time;

            // 将执行域与判定域物理隔离
            if (current_frame < 4) {
                if (current_frame % 2 == 0) servos.SetArms(90, 120);
                else servos.SetArms(90, 140);
                current_frame++;
            }
            else {
                // 只有当 current_frame 已经达到 4，且又经历了一个 750ms 周期后，
                // 才执行状态回收。这保证了最后一帧能够驻留完整的设定时间。
                TriggerMotion(MOTION_NULL);
            }
        }
        break;

    case MOTION_FOWD: // 3次，幅度-70°~0°，反向摆动，持续4秒 (666ms/帧)
        if (current_time - last_frame_time >= 666) {
            last_frame_time = current_time;
            if (current_frame % 2 == 0) servos.SetArms(20, 160);
            else servos.SetArms(90, 90);
            current_frame++;
            if (current_frame >= 6) TriggerMotion(MOTION_NULL);
        }
        break;
    }
}