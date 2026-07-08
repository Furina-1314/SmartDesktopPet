#include <WiFi.h>
#include <HTTPClient.h>
#include "ArduinoJson.h"
#include <Preferences.h>
#include <esp_sleep.h>

#include "core_sys.h"
#include "bsp_oled.h"
#include "app_emotion.h"
#include "app_interaction.h"
#include "app_motion.h"
#include "app_environment.h" 

// ==== 基础配置 ====
const char* WIFI_SSID = "fzl";
const char* WIFI_PASSWORD = "myserenades";
const uint32_t STANDBY_TIMEOUT_MS = 30000;
const uint32_t PRINT_INTERVAL_MS = 5000;

OLEDSystem screen;
EmotionSystem pet_emotion;
InteractionManager interaction;
MotionController pet_motion;
EnvironmentSystem pet_env;

const uint8_t ENCODER_KEY_PIN = 6;
const uint8_t ENCODER_S1_PIN = 16;
const uint8_t ENCODER_S2_PIN = 15;
const uint8_t SERVO_LEFT_PIN = 41;
const uint8_t SERVO_RIGHT_PIN = 42;
const uint8_t HW072_DO_PIN = 19;

// ==== Flash 与全局状态 ====
Preferences prefs_cfg;
Preferences prefs_mem;

uint32_t total_pets = 0;
uint32_t total_feeds = 0;

bool system_sleeping = false;

// ==== 提权到全局的时序控制变量（防止唤醒瞬间发生时间跳变） ====
uint32_t last_pet_time = 0;
uint32_t last_emo_drop = 0;
uint32_t last_auto_action = 0;
uint32_t last_weather_update = 0;
uint32_t last_print = 0;

// ==== RTC 连续性追踪变量 ====
RTC_DATA_ATTR uint32_t rtc_bootMs = 0;
RTC_DATA_ATTR uint32_t rtc_lastInteract = 0;
RTC_DATA_ATTR uint8_t rtc_wasSleeping = 0;   // 0:冷启动, 1:待机30s唤醒, 2:光敏睡眠唤醒
RTC_DATA_ATTR float current_temperature = -999.0;
RTC_DATA_ATTR int current_weather_code = -1;

uint32_t GetAbsoluteTimeMs() { return rtc_bootMs + millis(); }

// ==========================================
// 记忆系统与硬件级待机 (Deep-Sleep)
// ==========================================
void LoadPetMemory() {
    prefs_mem.begin("pet_mem", true);
    total_pets = prefs_mem.getUInt("pets", 0);
    total_feeds = prefs_mem.getUInt("feeds", 0);
    prefs_mem.end();

    if (rtc_wasSleeping > 0) {
        prefs_cfg.begin("pet_cfg", true);
        pet_emotion.SetEmotion(prefs_cfg.getUInt("emotion", 5));
        bool is_full = prefs_cfg.getBool("is_full", false);
        uint16_t remain = prefs_cfg.getUInt("full_rem", 0); // 【新增】读取剩余秒数
        prefs_cfg.end();

        // 【核心修正】拦截默认的 SetFull 行为，采用精准的 Restore 接口
        if (is_full && remain > 0) {
            pet_emotion.RestoreFullState(remain);
        }
        else {
            pet_emotion.SetFull(false);
        }
        LOG_INFO("Restored state and precise timers from Flash memory.");
    }
}

void GoToDeepSleep() {
    rtc_bootMs += millis();

    // 掉电前保存即时状态（写入 Flash）
    prefs_cfg.begin("pet_cfg", false);
    prefs_cfg.putUInt("emotion", pet_emotion.GetEmotion());
    prefs_cfg.putBool("is_full", pet_emotion.GetIsFull());

    // 【核心修正】同步将确切的剩余秒数写入 Flash 记忆空间
    prefs_cfg.putUInt("full_rem", pet_emotion.GetFullRemainSeconds());

    prefs_cfg.end();

    screen.Sleep();
    esp_sleep_enable_ext0_wakeup((gpio_num_t)ENCODER_KEY_PIN, 0);
    esp_deep_sleep_start();
}

// ==========================================
// 异步网络请求
// ==========================================
void InitNetwork() {
    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void FetchWeatherData() {
    if (WiFi.status() != WL_CONNECTED) return;
    HTTPClient http;
    http.begin("http://api.open-meteo.com/v1/forecast?latitude=39.9042&longitude=116.4074&current_weather=true");
    http.setTimeout(3000);
    if (http.GET() == HTTP_CODE_OK) {
        DynamicJsonDocument doc(2048);
        if (!deserializeJson(doc, http.getString())) {
            current_temperature = doc["current_weather"]["temperature"];
            current_weather_code = doc["current_weather"]["weathercode"];
        }
    }
    http.end();
}

// ==========================================
// 恢复定时器 (防止唤醒瞬间的补偿执行)
// ==========================================
void ResetTimersOnWakeup() {
    uint32_t now = millis();
    rtc_lastInteract = GetAbsoluteTimeMs();
    last_emo_drop = now;
    last_auto_action = now;
    last_print = now;
}

// ==========================================
// 串口指令解析系统
// ==========================================
void ProcessSerialDebug() {
    static char serial_buf[64];
    static uint8_t buf_idx = 0;

    while (Serial.available() > 0) {
        char c = Serial.read();

        if (c == '\n' || c == '\r') {
            serial_buf[buf_idx] = '\0';

            if (buf_idx > 0) {
                rtc_lastInteract = GetAbsoluteTimeMs();

                // 【规范 3】即使在睡眠态，也必须且仅响应 setslp
                if (strncmp(serial_buf, "setslp", 6) == 0) {
                    if (strstr(serial_buf, "true") != NULL && !system_sleeping) {
                        system_sleeping = true;
                        pet_emotion.SetSleep(true);
                        screen.Sleep();                         // 【规范 1】清空屏幕
                        pet_motion.TriggerMotion(MOTION_NULL);
                        pet_motion.ForceSleepPosture();         // 【规范 2】移至+80°
                        delay(600); // 等待舵机机械响应
                        Serial.printf("[%lu ms] Sleep Mode On\n", millis()); // 【规范 4】打印通知
                    }
                    else if (strstr(serial_buf, "false") != NULL && system_sleeping) {
                        system_sleeping = false;
                        pet_emotion.SetSleep(false);
                        screen.Wakeup();
                        ResetTimersOnWakeup();
                        // 【唤醒不回中立位】跳过 TriggerMotion
                        Serial.printf("[%lu ms] Sleep Mode Off\n", millis());
                    }
                }
                // 【规范 3】如果在睡眠态，静默抛弃除此之外的所有指令
                else if (system_sleeping) {
                    buf_idx = 0;
                    continue;
                }
                // 以下指令仅在清醒时响应
                else if (strncmp(serial_buf, "setemo", 6) == 0) {
                    int emo;
                    if (sscanf(serial_buf, "setemo %d", &emo) == 1) pet_emotion.SetEmotion((uint8_t)emo);
                }
                else if (strncmp(serial_buf, "setful", 6) == 0) {
                    if (strstr(serial_buf, "true") != NULL) pet_emotion.SetFull(true);
                    else if (strstr(serial_buf, "false") != NULL) pet_emotion.SetFull(false);
                }
                else if (strncmp(serial_buf, "setmot", 6) == 0) {
                    int mot;
                    if (sscanf(serial_buf, "setmot %d", &mot) == 1) pet_motion.TriggerMotion((PetMotion)mot);
                }
                buf_idx = 0;
            }
        }
        else if (buf_idx < sizeof(serial_buf) - 1) {
            serial_buf[buf_idx++] = c;
        }
    }
}

// ==========================================
// 系统初始化
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(500);

    screen.Init();
    pet_emotion.Init();
    interaction.Init(ENCODER_KEY_PIN, ENCODER_S1_PIN, ENCODER_S2_PIN);
    pet_motion.Init(SERVO_LEFT_PIN, SERVO_RIGHT_PIN);
    pet_env.Init(HW072_DO_PIN);

    LoadPetMemory();

    if (rtc_wasSleeping == 2) {
        Serial.println("[Boot] Woke up from Light Sleep. Posture held."); // 光敏唤醒，不回中立位
    }
    else {
        pet_motion.TriggerMotion(MOTION_NULL);
        if (rtc_wasSleeping == 0) InitNetwork();
    }

    ResetTimersOnWakeup();
    rtc_wasSleeping = 0;
}

// ==========================================
// 时域主循环
// ==========================================
void loop() {
    uint32_t current_abs_time = GetAbsoluteTimeMs();
    uint32_t now = millis();

    // 1. 串口解析始终在后台监听唤醒信号
    ProcessSerialDebug();

    // 2. 交互按键检测
    InteractionEvent event = interaction.Update(system_sleeping);

    // ===============================================
    // 🛡️ 【系统软隔离屏障】软睡眠模式下的状态机拦截
    // ===============================================
    if (system_sleeping) {
        if (event == EVENT_WAKEUP) { // 仅允许按钮唤醒操作
            system_sleeping = false;
            pet_emotion.SetSleep(false);
            screen.Wakeup();
            ResetTimersOnWakeup();
            Serial.printf("[%lu ms] Woke up from soft sleep by Button.\n", now);
        }
        // 【核心机制】：如果没被唤醒，直接 return！
        // 这一刀彻底切断了吃饱计时、心情掉落、自动动作以及打印，完美满足 3, 4, 5, 6 规范。
        return;
    }

    // ===============================================
    // 以下为系统清醒状态 (Active) 下的日常轮询
    // ===============================================

    // [时序 1] 30秒无操作硬件待机判定
    if (current_abs_time - rtc_lastInteract >= STANDBY_TIMEOUT_MS) {
        rtc_wasSleeping = 1;
        GoToDeepSleep();
    }

    // [常规采样]
    pet_emotion.Update();     // 更新饱食度倒计时
    pet_motion.Update(false); // 更新舵机插值运算

    if (event != EVENT_NONE) rtc_lastInteract = current_abs_time;

    // [交互逻辑与 Flash 闭环]
    if (event == EVENT_PET_SUCCESS) {
        uint32_t cooldown = 10000 - (total_pets / 5) * 1000;
        if (cooldown < 2000 || cooldown > 10000) cooldown = 2000;

        if (now - last_pet_time >= cooldown) {
            last_pet_time = now;
            total_pets++;
            prefs_mem.begin("pet_mem", false);
            prefs_mem.putUInt("pets", total_pets);
            prefs_mem.end();

            Serial.println("Head Touch");
            pet_motion.TriggerMotion(MOTION_PLAY);

            uint8_t current_emo = pet_emotion.GetEmotion();
            uint8_t max_emo = 10 + (total_pets / 10);
            if (current_emo < (max_emo > 20 ? 20 : max_emo)) {
                pet_emotion.SetEmotion(current_emo + 1);
            }
        }
    }
    else if (event == EVENT_FEED_SUCCESS) {
        total_feeds++;
        prefs_mem.begin("pet_mem", false);
        prefs_mem.putUInt("feeds", total_feeds);
        prefs_mem.end();
        Serial.println("Feed");
        pet_emotion.Feed();
    }

    // [光敏低光照] 触发硬件睡眠
    if (pet_env.CheckSleepCondition(false)) {
        Serial.println("[Sys] Low Light Triggered. Deep Sleep.");
        pet_motion.TriggerMotion(MOTION_NULL);
        pet_motion.ForceSleepPosture(); // 移至+80°
        delay(600);
        rtc_wasSleeping = 2;
        GoToDeepSleep();
    }

    // [时序 2] 心情衰减
    if (!pet_emotion.GetIsFull() && (now - last_emo_drop >= 30000)) {
        last_emo_drop = now;
        uint8_t cur = pet_emotion.GetEmotion();
        if (cur > 0) pet_emotion.SetEmotion(cur - 1);
    }

    // [时序 3] 自动动作
    if (pet_motion.GetCurrentMotion() == MOTION_NULL && (now - last_auto_action >= 1000)) {
        last_auto_action = now;
        float prob = 3.33f + (total_pets / 2) * 0.1f;
        if (random(10000) < ((prob > 10.0f ? 10.0f : prob) * 100)) {
            pet_motion.TriggerMotion(MOTION_IDLE);
        }
    }

    // ===============================================
        // [网络异步] 高频状态轮询与边沿检测重连机制
        // ===============================================
    static uint32_t last_wifi_check = 0;
    static bool last_wifi_state = false; // 用于存储上一状态，构建边沿触发器

    // 将网卡状态轮询周期缩短至 5000ms (5秒)
    if (now - last_wifi_check >= 5000) {
        last_wifi_check = now;
        bool current_wifi_state = (WiFi.status() == WL_CONNECTED);

        // 1. 下降沿触发：检测到网络由连接变为断开
        if (!current_wifi_state && last_wifi_state) {
            Serial.println("[Net] Warning: WiFi Disconnected. Triggering explicit reconnect...");
            WiFi.disconnect(); // 清理残余的 Socket 句柄与底层状态
            WiFi.reconnect();  // 触发一次非阻塞的重新握手
            last_wifi_state = false;
        }
        // 2. 上升沿触发：检测到网络由断开变为连接
        else if (current_wifi_state && !last_wifi_state) {
            Serial.println("[Net] Info: WiFi Reconnected successfully.");
            last_wifi_state = true;
        }

        // 3. 稳态执行：仅在网络处于稳定连接态时，执行气象数据的 10 分钟 (600000ms) 周期刷新
        if (current_wifi_state) {
            if (last_weather_update == 0 || (current_abs_time - last_weather_update >= 600000)) {
                FetchWeatherData();
                last_weather_update = current_abs_time;
            }
        }
    }

    // [规范打印] 每5秒日志
    if (now - last_print >= PRINT_INTERVAL_MS) {
        last_print = now;
        char log_buf[128];
        snprintf(log_buf, sizeof(log_buf), "[%lu ms] EMOTION:%d %s ACTION:%d | Head:%d Feed:%d",
            now, pet_emotion.GetEmotion(),
            pet_emotion.GetIsFull() ? "FULL" : "HUNGRY",
            pet_motion.GetCurrentMotion(), total_pets, total_feeds);
        Serial.println(log_buf);
    }

    // 屏幕渲染管线
    screen.UpdateDesktopPetUI(
        pet_emotion.GetEmotion(), pet_emotion.GetEmotionKaomoji(),
        pet_emotion.GetIsFull(), pet_emotion.GetFullRemainSeconds(),
        pet_emotion.GetStateKaomoji(), pet_motion.GetCurrentMotion(),
        current_temperature, current_weather_code
    );
}