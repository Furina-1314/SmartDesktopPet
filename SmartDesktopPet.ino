#include <WiFi.h>
#include <HTTPClient.h>
#include "ArduinoJson.h"
#include <Preferences.h>
#include <esp_sleep.h>
#include <sys/time.h> // 【新增】用于获取包含 Deep Sleep 的绝对系统时间

#include "core_sys.h"
#include "bsp_oled.h"
#include "app_emotion.h"
#include "app_interaction.h"
#include "app_motion.h"
#include "app_environment.h" 

// ==== 基础配置 ====
const char* WIFI_SSID = "fzl";
const char* WIFI_PASSWORD = "MySerenades";
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
uint32_t last_auto_action = 0;
uint32_t last_weather_update = 0;
uint32_t last_print = 0;

// ==== RTC 连续性追踪变量 ====
RTC_DATA_ATTR uint32_t rtc_lastInteract = 0;
RTC_DATA_ATTR uint8_t rtc_wasSleeping = 0;   // 0:冷启动, 1:待机30s唤醒, 2:光敏睡眠唤醒
RTC_DATA_ATTR float current_temperature = -999.0;
RTC_DATA_ATTR int current_weather_code = -1;

// 【重写】获取自首次上电以来的绝对毫秒数（无缝跨越 Deep Sleep）
uint32_t GetAbsoluteTimeMs() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint32_t)(tv.tv_sec * 1000ULL + tv.tv_usec / 1000ULL);
}

// ==========================================
// 记忆系统与硬件级待机 (Deep-Sleep)
void GoToDeepSleep() {
    prefs_cfg.begin("pet_cfg", false);
    prefs_cfg.putUInt("emotion", pet_emotion.GetEmotion());
    prefs_cfg.putBool("is_full", pet_emotion.GetIsFull());

    // 固化饱食与心情的精确倒计时
    prefs_cfg.putInt("full_timer", pet_emotion.GetFullTimerMs());
    prefs_cfg.putInt("decay_timer", pet_emotion.GetDecayTimerMs());
    prefs_cfg.end();

    screen.Sleep();
    esp_sleep_enable_ext0_wakeup((gpio_num_t)ENCODER_KEY_PIN, 0);
    delay(50); // 确保 Flash 物理写入完成
    esp_deep_sleep_start();
}

void LoadPetMemory() {
    prefs_mem.begin("pet_mem", true);
    total_pets = prefs_mem.getUInt("pets", 0);
    total_feeds = prefs_mem.getUInt("feeds", 0);
    prefs_mem.end();

    if (rtc_wasSleeping > 0) {
        prefs_cfg.begin("pet_cfg", true);
        pet_emotion.SetEmotion(prefs_cfg.getUInt("emotion", 5));

        // 恢复精确的倒计时状态
        pet_emotion.SetFullTimerMs(prefs_cfg.getInt("full_timer", 0));
        pet_emotion.SetDecayTimerMs(prefs_cfg.getInt("decay_timer", 30000));
        prefs_cfg.end();
        LOG_INFO("Restored precision state from Flash memory.");
    }
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
    last_auto_action = now;
    last_print = now;
}

// ==========================================
// 串口指令解析系统
// ==========================================
// 将返回类型改为 bool，仅表示是否捕获到有效交互
bool ProcessSerialDebug() {
    static char serial_buf[64];
    static uint8_t buf_idx = 0;
    bool activity_detected = false; // 新增状态标识，默认无交互

    while (Serial.available() > 0) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            serial_buf[buf_idx] = '\0';
            if (buf_idx > 0) {
                activity_detected = true; // 标记产生有效交互事件

                // 【规范 3】即使在睡眠态，也必须且仅响应 setslp
                if (strncmp(serial_buf, "setslp", 6) == 0) {
                    if (strstr(serial_buf, "true") != NULL && !system_sleeping) {
                        system_sleeping = true;
                        pet_emotion.SetSleep(true);
                        screen.Sleep();
                        pet_motion.TriggerMotion(MOTION_NULL);
                        pet_motion.ForceSleepPosture();
                        delay(600);
                        Serial.printf("[%lu ms] Sleep Mode On\n", GetAbsoluteTimeMs());
                    }
                    else if (strstr(serial_buf, "false") != NULL && system_sleeping) {
                        system_sleeping = false;
                        pet_emotion.SetSleep(false);
                        screen.Wakeup();
                        ResetTimersOnWakeup();
                        Serial.printf("[%lu ms] Sleep Mode Off\n", GetAbsoluteTimeMs());
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
                    LOG_MOTION(mot);
                }
                buf_idx = 0;
            }
        }
        else if (buf_idx < sizeof(serial_buf) - 1) {
            serial_buf[buf_idx++] = c;
        }
    }

    // 【核心修复】：函数末尾必须确保返回状态，避免触发栈空间与寄存器未定义异常
    return activity_detected;
}
// ==========================================
// 系统初始化
// ==========================================
void setup() {

    // 测试用
    // 物理层擦除命令
    //prefs_mem.begin("pet_mem", false);
    //prefs_mem.clear(); // 彻底擦除该命名空间下的所有键值节点
    //prefs_mem.end();
    //


    Serial.begin(115200);
    delay(500);

    screen.Init();
    pet_emotion.Init();
    interaction.Init(ENCODER_KEY_PIN, ENCODER_S1_PIN, ENCODER_S2_PIN);
    pet_motion.Init(SERVO_LEFT_PIN, SERVO_RIGHT_PIN);
    pet_env.Init(HW072_DO_PIN);

    LoadPetMemory();

    // 【刚需保留】DeepSleep 唤醒后 SRAM 清空，必须无条件重新初始化基带与网络栈
    InitNetwork();

    // 【状态收敛】无论是冷启动、30秒待机唤醒，还是低光照唤醒，统一强制回中立位
    pet_motion.TriggerMotion(MOTION_NULL);

    ResetTimersOnWakeup();
    rtc_wasSleeping = 0;
}

// ==========================================
// 时域主循环
// ==========================================
void loop() {
    uint32_t current_abs_time = GetAbsoluteTimeMs();
    uint32_t now = millis();

    // 1. 采集子系统事件
    bool serial_active = ProcessSerialDebug();
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
            Serial.printf("[%lu ms] Woke up from soft sleep by Button.\n", current_abs_time);
        }
        // 【核心机制】：如果没被唤醒，直接 return！
        // 这一刀彻底切断了吃饱计时、心情掉落、自动动作以及打印，完美满足 3, 4, 5, 6 规范。
        return;
    }

    // 2. 统一的时基同步枢纽 (消除脏写)
    if (serial_active || event != EVENT_NONE) {
        rtc_lastInteract = current_abs_time;
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

    // ===============================================
        // [交互逻辑与 Flash 闭环] - 结合记忆算法 1 与 2
        // ===============================================
    if (event == EVENT_PET_SUCCESS) {

        // 算法 2：多巴胺反馈衰减 (T_cd)
        // 基础冷却 10000ms，每 5 次摸头减少 1000ms，硬性下限 2000ms
        uint32_t cd_reduction = (total_pets / 5) * 1000;
        // 防下溢 (Underflow) 设计：当减量超过或等于 8000 时，直接截断至下限
        uint32_t cooldown = (cd_reduction >= 8000) ? 2000 : (10000 - cd_reduction);

        if (now - last_pet_time >= cooldown) {
            last_pet_time = now;

            // 更新 Flash (NVS) 记忆
            total_pets++;
            prefs_mem.begin("pet_mem", false);
            prefs_mem.putUInt("pets", total_pets);
            prefs_mem.end();

            Serial.printf("[%lu ms] Head Touch\n", current_abs_time);
            pet_motion.TriggerMotion(MOTION_PLAY);

            // 算法 1：情感上限阈值解锁 (E_max)
            uint8_t current_emo = pet_emotion.GetEmotion();
            // 基础上限 10，每 10 次交互提升 1 点，绝对物理上限 20
            uint8_t dynamic_max_emo = 10 + (total_pets / 10);
            if (dynamic_max_emo > 20) dynamic_max_emo = 20;

            if (current_emo < dynamic_max_emo) {
                pet_emotion.SetEmotion(current_emo + 1);
            }
        }
    }
    else if (event == EVENT_FEED_SUCCESS) {
        total_feeds++;
        prefs_mem.begin("pet_mem", false);
        prefs_mem.putUInt("feeds", total_feeds);
        prefs_mem.end();
        Serial.printf("[%lu ms] Feed\n", current_abs_time);
        pet_emotion.Feed();
    }
    // [光敏低光照] 触发硬件睡眠
    if (pet_env.CheckSleepCondition(false)) {
        Serial.printf("[%lu ms] [Sys] Low Light Triggered. Deep Sleep.\n", current_abs_time);
        screen.Sleep();
        pet_motion.ForceSleepPosture(); // 移至+80°
        delay(600);
        rtc_wasSleeping = 2;
        GoToDeepSleep();
    }

    // ===============================================
        // [时序 3] 自动动作 - 结合记忆算法 3
        // ===============================================
    if (pet_motion.GetCurrentMotion() == MOTION_NULL && (now - last_auto_action >= 1000)) {
        last_auto_action = now;

        // 算法 3：自主活跃度概率增益 (P_auto)
        // 采用万分率整数域计算：基础概率 333 (3.33%)，每次交互增加 5 (0.05%)
        uint32_t prob_threshold = 333 + (total_pets * 5);
        // 上限阈值截断：最高 1000 (10.0%)
        if (prob_threshold > 1000) prob_threshold = 1000;

        // 随机事件生成器 (RNG) 命中判定
        if (random(10000) < prob_threshold) {
            // 引入选修设计状态机：判断是否疲惫
            if (pet_emotion.GetEmotion() >= 5) {
                pet_motion.TriggerMotion(MOTION_IDLE);
            }
            else {
                pet_motion.TriggerMotion(MOTION_TIRE);
            }
        }
    }
    // ===============================================
        // [网络异步] 5秒定时轮询与断线自恢复状态机
        // ===============================================
    static uint32_t last_wifi_check = 0;

    if (now - last_wifi_check >= 5000) {
        last_wifi_check = now;

        if (WiFi.status() != WL_CONNECTED) {
            Serial.printf("[%lu ms] [Net] Status: Disconnected. Initiating reconnect sequence...\n", current_abs_time);
            // 采用 disconnect() 清理残留 Socket 句柄，随后重新请求基带关联
            WiFi.disconnect();
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        }
        else {
            // 网络处于稳态，执行 HTTP 气象数据获取 (周期 10 分钟)
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
            current_abs_time, pet_emotion.GetEmotion(),
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