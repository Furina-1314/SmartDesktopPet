#include "app_emotion.h"

EmotionSystem::EmotionSystem() { Init(); }

void EmotionSystem::Init() {
    current_emotion = 5;
    is_full = false;
    is_sleeping = false;
    last_update_time = millis();
    decay_timer_ms = DECAY_INTERVAL_MS;
    full_timer_ms = 0;
}

void EmotionSystem::Update() {
    uint32_t current_time = millis();
    uint32_t delta_t = current_time - last_update_time;
    last_update_time = current_time;

    if (is_sleeping) return;

    if (is_full) {
        full_timer_ms -= delta_t;
        if (full_timer_ms <= 0) {
            is_full = false;
            full_timer_ms = 0;
        }
    }
    else {
        decay_timer_ms -= delta_t;
        if (decay_timer_ms <= 0) {
            decay_timer_ms = DECAY_INTERVAL_MS;
            if (current_emotion > MIN_EMOTION) current_emotion--;
        }
    }
}

void EmotionSystem::Feed() {
    is_full = true;
    full_timer_ms = FULL_DURATION_MS;
}

void EmotionSystem::Pet() {
    if (current_emotion < MAX_EMOTION) current_emotion++;
}

void EmotionSystem::SetSleep(bool sleep_state) {
    is_sleeping = sleep_state;
    if (!sleep_state) last_update_time = millis();
}

void EmotionSystem::SetEmotion(uint8_t val) {
    if (val >= MIN_EMOTION && val <= MAX_EMOTION) current_emotion = val;
}

void EmotionSystem::SetFull(bool state) {
    is_full = state;
    if (state) full_timer_ms = FULL_DURATION_MS;
    else full_timer_ms = 0;
}

uint16_t EmotionSystem::GetFullRemainSeconds() const {
    return is_full ? (uint16_t)(full_timer_ms / 1000) : 0;
}

const char* EmotionSystem::GetEmotionKaomoji() const {
    if (current_emotion >= 12) return"(> w <)";
    if (current_emotion >= 8) return "(*^3^*)";
    if (current_emotion >= 5) return "( ^ v ^ )";
    if (current_emotion >= 3) return " ( 0 ^ 0`) ";
    return "(T_T)";
}

const char* EmotionSystem::GetStateKaomoji() const {
    if (is_sleeping) return "( -_-)zZ";
    return is_full ? "( ^ O ^)c[]" : "( O_O)!!";
}

void EmotionSystem::RestoreFullState(uint16_t remain_sec) {
    if (remain_sec > 0) {
        // 恢复布尔状态与精确定量时间
        is_full = true;
        full_timer_ms = remain_sec;

        // 【关键】刷新时间基准锚点
        // 如果您的类内部有类似 last_update_time 或 full_start_time 的变量用于计算 1 秒流逝，
        // 请务必在这里将其赋值为 millis()，以防唤醒后发生时间偏移。
        // 例如：last_update_time = millis(); 
    }
    else {
        is_full = false;
        full_timer_ms = 0;
    }
}