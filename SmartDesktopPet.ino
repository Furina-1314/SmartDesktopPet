#include "core_sys.h"
#include "bsp_oled.h"
#include "app_emotion.h"
#include "app_interaction.h"
#include "app_motion.h"

OLEDSystem screen;
EmotionSystem pet_emotion;
InteractionManager interaction;
MotionController pet_motion;

const uint8_t ENCODER_KEY_PIN = 6;
const uint8_t ENCODER_S1_PIN = 16;
const uint8_t ENCODER_S2_PIN = 15;
const uint8_t SERVO_LEFT_PIN = 41;
const uint8_t SERVO_RIGHT_PIN = 42;

bool system_sleeping = false;
uint32_t last_print_time = 0;
const uint32_t PRINT_INTERVAL_MS = 5000;

void ProcessSerialDebug() {
    static char serial_buf[64];
    static uint8_t buf_idx = 0;
    while (Serial.available() > 0) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            serial_buf[buf_idx] = '\0';
            if (buf_idx > 0) {
                if (strncmp(serial_buf, "setemo", 6) == 0) {
                    int emo_val;
                    if (sscanf(serial_buf, "setemo %d", &emo_val) == 1) {
                        pet_emotion.SetEmotion((uint8_t)emo_val);
                        Serial.printf("[%lu ms] Debug: Emotion set to %d\n", millis(), emo_val);
                    }
                }
                else if (strncmp(serial_buf, "setful", 6) == 0) {
                    if (strstr(serial_buf, "true") != NULL) {
                        pet_emotion.SetFull(true);
                        Serial.printf("[%lu ms] Debug: Force FULL\n", millis());
                    }
                    else if (strstr(serial_buf, "false") != NULL) {
                        pet_emotion.SetFull(false);
                        Serial.printf("[%lu ms] Debug: Force HUNGRY\n", millis());
                    }
                }
                else if (strncmp(serial_buf, "setmot", 6) == 0) {
                    int mot_id;
                    if (sscanf(serial_buf, "setmot %d", &mot_id) == 1) {
                        pet_motion.TriggerMotion(static_cast<PetMotion>(mot_id));
                        Serial.printf("[%lu ms] Motion %d\n", millis(), mot_id);
                    }
                }
                else if (strncmp(serial_buf, "setslp", 6) == 0) {
                    if (strstr(serial_buf, "true") != NULL) {
                        system_sleeping = true;
                        pet_emotion.SetSleep(true);
                        screen.Sleep();
                        pet_motion.TriggerMotion(MOTION_NULL);
                        Serial.printf("[%lu ms] Sleep Mode On\n", millis());
                    }
                    else if (strstr(serial_buf, "false") != NULL) {
                        system_sleeping = false;
                        pet_emotion.SetSleep(false);
                        screen.Wakeup();
                        Serial.printf("[%lu ms] Sleep Mode Off\n", millis());
                    }
                }
                buf_idx = 0;
            }
        }
        else if (buf_idx < sizeof(serial_buf) - 1) {
            serial_buf[buf_idx++] = c;
        }
    }
}

void setup() {
    Serial.begin(115200);
    screen.Init();
    pet_emotion.Init();
    interaction.Init(ENCODER_KEY_PIN, ENCODER_S1_PIN, ENCODER_S2_PIN);
    pet_motion.Init(SERVO_LEFT_PIN, SERVO_RIGHT_PIN);
    LOG_INFO("System Boot Done.");
}

void loop() {
    uint32_t current_time = millis();
    ProcessSerialDebug();
    pet_emotion.Update();
    pet_motion.Update(system_sleeping);

    InteractionEvent event = interaction.Update(system_sleeping);
    if (event == EVENT_WAKEUP) {
        system_sleeping = false;
        pet_emotion.SetSleep(false);
        screen.Wakeup();
        pet_motion.TriggerMotion(MOTION_IDLE);
    }
    else if (event == EVENT_PET_SUCCESS) {
        pet_emotion.Pet();
        pet_motion.TriggerMotion(MOTION_PLAY);
    }
    else if (event == EVENT_FEED_SUCCESS) {
        pet_emotion.Feed();
    }

    pet_motion.UpdateAutonomousBehavior(pet_emotion.GetEmotion(), system_sleeping);

    if (!system_sleeping && (current_time - last_print_time >= PRINT_INTERVAL_MS)) {
        last_print_time = current_time;
        char status_buf[64];
        if (pet_emotion.GetIsFull()) {
            snprintf(status_buf, sizeof(status_buf), "EMOTION: %d | FULL %02d Min %02ds | MOTION: %d",
                pet_emotion.GetEmotion(), pet_emotion.GetFullRemainSeconds() / 60,
                pet_emotion.GetFullRemainSeconds() % 60, pet_motion.GetCurrentMotion());
        }
        else {
            snprintf(status_buf, sizeof(status_buf), "EMOTION: %d | HUNGRY | MOTION: %d",
                pet_emotion.GetEmotion(), pet_motion.GetCurrentMotion());
        }
        LOG_INFO(status_buf);
    }

    if (!system_sleeping) {
        screen.UpdateDesktopPetUI(pet_emotion.GetEmotion(), pet_emotion.GetEmotionKaomoji(),
            pet_emotion.GetIsFull(), pet_emotion.GetFullRemainSeconds(),
            pet_emotion.GetStateKaomoji(), pet_motion.GetCurrentMotion());
    }
}