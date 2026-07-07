#include "bsp_oled.h"

OLEDSystem::OLEDSystem() : u8g2(U8G2_R0, U8X8_PIN_NONE, 4, 5) {
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

void OLEDSystem::UpdateDesktopPetUI(uint8_t emotion, const char* emoKaomoji, bool isFull, uint16_t remainSeconds, const char* stateKaomoji, PetMotion motion) {
    u8g2.clearBuffer();

    char buffer[64]; // 稍微扩大缓冲，防止 UTF-8 多字节字符溢出

    // 第一行：心情值与心情颜文字 
    // 排版预期：Emo: 5 (*^▽^*)
    snprintf(buffer, sizeof(buffer), "Emo:%d %s", emotion, emoKaomoji);
    DrawUTF8Text(0, 16, buffer);

    // 第二行：饱食状态与状态颜文字
    // 排版预期：Full 02:00 ( ˘▽˘)c[]
    if (isFull) {
        uint8_t min = remainSeconds / 60;
        uint8_t sec = remainSeconds % 60;
        snprintf(buffer, sizeof(buffer), "Full %02d:%02d %s", min, sec, stateKaomoji);
    }
    else {
        snprintf(buffer, sizeof(buffer), "Hungry %s", stateKaomoji);
    }
    DrawUTF8Text(0, 38, buffer);

    // 第三行：动作状态
    snprintf(buffer, sizeof(buffer), "Act: %s", GetMotionString(motion));
    DrawUTF8Text(0, 60, buffer);

    u8g2.sendBuffer();
}