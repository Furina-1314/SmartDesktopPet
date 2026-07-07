#pragma once
#ifndef __BSP_OLED_H__
#define __BSP_OLED_H__

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "core_sys.h" // 引入全局的 PetMotion 和其他核心宏

class OLEDSystem {
private:
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

    void DrawUTF8Text(uint8_t x, uint8_t y, const char* text);
    const char* GetMotionString(PetMotion motion);

public:
    OLEDSystem();
    void Init();
    void Clear();
    void Sleep();
    void Wakeup();

    void UpdateDesktopPetUI(
        uint8_t emotion,
        const char* emoKaomoji,
        bool isFull,
        uint16_t remainSeconds,
        const char* stateKaomoji,
        PetMotion motion
    );
};

#endif // __BSP_OLED_H__