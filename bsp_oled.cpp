#include "bsp_oled.h"

static const char* GetWeatherDescription(int code) {
    if (code == 0) return "晴";
    if (code == 1 || code == 2 || code == 3) return "多云";
    if (code == 45 || code == 48) return "雾";
    if (code >= 51 && code <= 67) return "雨";
    if (code >= 71 && code <= 77) return "雪";
    if (code >= 80 && code <= 82) return "阵雨";
    if (code >= 85 && code <= 86) return "阵雪";
    if (code >= 95 && code <= 99) return "雷雨";
    return "未知"; // 越界容错
}

OLEDSystem::OLEDSystem() : u8g2(U8G2_R0, U8X8_PIN_NONE, 4, 5) {// 4: SCL, 5: SDA
}

void OLEDSystem::Init() {
    u8g2.begin();
    // 【核心修改 1】激活 U8g2 的 UTF-8 字符解码支持
    u8g2.enableUTF8Print();

    u8g2.setFontMode(1);
    u8g2.setDrawColor(1);
}

void OLEDSystem::Clear() {
    u8g2.clearBuffer();
    u8g2.sendBuffer();
}

void OLEDSystem::Sleep() {
    u8g2.setPowerSave(1);
}

void OLEDSystem::Wakeup() {
    u8g2.setPowerSave(0);
}

void OLEDSystem::DrawUTF8Text(uint8_t x, uint8_t y, const char* text) {
    // 替换为文泉驿12号点阵字库（包含 GB2312 常用字与全角符号）
    u8g2.setFont(u8g2_font_wqy12_t_gb2312);
    u8g2.drawUTF8(x, y, text);
}
const char* OLEDSystem::GetMotionString(PetMotion motion) {
    switch (motion) {
    case MOTION_NULL: return "NULL";
    case MOTION_PLAY: return "PLAY";
    case MOTION_IDLE: return "IDLE";
    case MOTION_TIRE: return "TIRE";
    case MOTION_FOWD: return "FOWD";
    default:          return "NULL";
    }
}


void OLEDSystem::UpdateDesktopPetUI(uint8_t emotion, const char* emoKaomoji,
    bool isFull, uint16_t remainSeconds,
    const char* stateKaomoji, PetMotion motion,
    float temp_c, int weather_code) {
    u8g2.clearBuffer();

    char buffer[64];

    // 第一行：心情值与颜文字 (Y=14)
    snprintf(buffer, sizeof(buffer), "Emo:%d %s", emotion, emoKaomoji);
    DrawUTF8Text(0, 14, buffer);

    // 第二行：饱食状态 (Y=30)
    if (isFull) {
        uint8_t min = remainSeconds / 60;
        uint8_t sec = remainSeconds % 60;
        snprintf(buffer, sizeof(buffer), "Full %02d:%02d %s", min, sec, stateKaomoji);
    }
    else {
        snprintf(buffer, sizeof(buffer), "Hungry %s", stateKaomoji);
    }
    DrawUTF8Text(0, 30, buffer);

    // 第三行：动作状态 (Y=46)
    snprintf(buffer, sizeof(buffer), "Act: %s", GetMotionString(motion));
    DrawUTF8Text(0, 46, buffer);

    // ==== 第四行：重新格式化的气象信息 (Y=62) ====
    if (temp_c > -900.0) {
        // 利用字典函数解析状态码，%.0f 代表去除小数位以保持界面整洁
        snprintf(buffer, sizeof(buffer), "北京  %s  %.0f℃", GetWeatherDescription(weather_code), temp_c);
    }
    else {
        snprintf(buffer, sizeof(buffer), "北京  获取中..."); // 网络未连接时的占位符
    }
    DrawUTF8Text(0, 62, buffer);

    u8g2.sendBuffer();
}