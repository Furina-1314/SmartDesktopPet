#pragma once
#ifndef __APP_EMOTION_H__
#define __APP_EMOTION_H__

#include <Arduino.h>

class EmotionSystem {
private:
    uint8_t current_emotion;
    bool is_full;
    bool is_sleeping;
    uint32_t last_update_time;
    int32_t  decay_timer_ms;
    int32_t  full_timer_ms;

    static const int32_t DECAY_INTERVAL_MS = 30000;
    static const int32_t FULL_DURATION_MS = 120000;
    static const uint8_t MAX_EMOTION = 20;
    static const uint8_t MIN_EMOTION = 1;

public:
    EmotionSystem();
    void Init();
    void Update();
    void Feed();
    void Pet();
    void SetSleep(bool sleep_state);
    void SetEmotion(uint8_t val);
    void SetFull(bool state);
    uint8_t GetEmotion() const { return current_emotion; }
    bool GetIsFull() const { return is_full; }
    uint16_t GetFullRemainSeconds() const;
    const char* GetEmotionKaomoji() const;
    const char* GetStateKaomoji() const;
    // 【新增】专用于 Deep-sleep 唤醒后的饱食倒计时精准重载
    void RestoreFullState(uint16_t remain_sec);
    // 新增精确时间戳接口
    int32_t GetFullTimerMs() const { return full_timer_ms; }
    void SetFullTimerMs(int32_t ms) {
        full_timer_ms = ms;
        is_full = (ms > 0);
    }
    // 获取/设置当前衰减计时器余量
    int32_t GetDecayTimerMs() const { return decay_timer_ms; }
    void SetDecayTimerMs(int32_t ms) {
        if (ms > 0 && ms <= DECAY_INTERVAL_MS) decay_timer_ms = ms;
    }
};
#endif