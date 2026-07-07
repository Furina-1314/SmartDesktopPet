#include "app_motion.h"
#include <stdlib.h>

void MotionController::Init(uint8_t pin_left, uint8_t pin_right) {
    pinL = pin_left;
    pinR = pin_right;
    servos.Init(pinL, pinR);

    current_motion = MOTION_NULL;
    current_frame = 0;
    last_frame_time = millis();
    last_idle_time = millis(); // 初始化空闲时间戳
}

void MotionController::TriggerMotion(PetMotion new_motion) {
    current_motion = new_motion;
    current_frame = 0; // 重置帧计数器
    last_frame_time = millis(); // 刷新时间戳，立即触发第0帧
    last_idle_time = millis(); // 【新增】动作打断时，重置空闲计时
    LOG_MOTION(new_motion); // 利用我们在 core_sys.h 定义的宏打印日志
}

void MotionController::UpdateAutonomousBehavior(uint8_t emotion, bool is_sleeping) {
    // 互斥条件：睡眠中，或正在执行动作时，暂停自动触发逻辑
    if (is_sleeping || current_motion != MOTION_NULL) {
        last_idle_time = millis();
        return;
    }

    uint32_t current_time = millis();
    // 严格满足大作业要求：每秒钟 (1000ms) 判定一次
    if (current_time - last_idle_time >= 1000) {
        last_idle_time = current_time;

        // 生成 0 到 9999 的随机数，3.33% 的概率等价于数值落在 [0, 332] 区间
        if (random(0, 10000) < 333) {
            // 严格遵循阈值判定
            if (emotion >= 5) {
                TriggerMotion(MOTION_IDLE); // 心情 >= 5 触发闲逛
            }
            else {
                TriggerMotion(MOTION_TIRE); // 心情 < 5 触发疲惫
            }
        }
    }
}

void MotionController::Update(bool is_sleeping) {
    // 睡眠保护逻辑：立刻切断舵机电源并挂起状态机
    if (is_sleeping) {
        servos.Detach();
        return;
    }
    // 清醒时确保舵机在线
    else {
        servos.Attach(pinL, pinR);
    }

    // 执行帧动画逻辑
    ExecuteFrame(millis());
}

void MotionController::ExecuteFrame(uint32_t current_time) {
    // 动作脚本引擎
    switch (current_motion) {

    case MOTION_NULL:
        // 空闲状态：回归 90 度并维持
        if (current_frame == 0) {
            servos.SetArms(90, 90);
            current_frame++; // 进入待机帧
        }
        break;

    case MOTION_PLAY:
        // 玩耍动作：双手挥舞 3 次 (每 300ms 切换一次方向)
        if (current_time - last_frame_time >= 300) {
            last_frame_time = current_time;

            if (current_frame % 2 == 0) {
                servos.SetArms(45, 135); // 举起
            }
            else {
                servos.SetArms(135, 45); // 放下
            }

            current_frame++;
            // 挥舞 6 次 (3个来回) 后，动作结束，回归 NULL
            if (current_frame >= 6) {
                TriggerMotion(MOTION_NULL);
            }
        }
        break;

    case MOTION_TIRE:
        // 疲惫动作：双手无力下垂
        if (current_frame == 0) {
            servos.SetArms(30, 30); // 向下低落
            current_frame++;
        }
        break;

    case MOTION_FOWD:
        // 前进动作：类似人走路的摆臂，交替挥动
        if (current_time - last_frame_time >= 500) {
            last_frame_time = current_time;
            if (current_frame % 2 == 0) {
                servos.SetArms(60, 60);
            }
            else {
                servos.SetArms(120, 120);
            }
            current_frame++;
            if (current_frame >= 4) {
                TriggerMotion(MOTION_NULL); // 走两步后停止
            }
        }
        break;

    case MOTION_IDLE:
        // 闲逛：随机的小幅度摆动
        if (current_time - last_frame_time >= 800) {
            last_frame_time = current_time;
            int randL = 90 + random(-20, 20);
            int randR = 90 + random(-20, 20);
            servos.SetArms(randL, randR);
            current_frame++;
            if (current_frame >= 5) {
                TriggerMotion(MOTION_NULL);
            }
        }
        break;
    }
}