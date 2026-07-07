#include "bsp_encoder.h"

static volatile int32_t encoder_counter = 0;
static volatile uint8_t prev_state = 0;
static uint8_t encoder_pin_a = 0;
static uint8_t encoder_pin_b = 0;

// 【核心：状态转移矩阵】
// 索引计算方法：(前一个状态 << 2) | 当前状态
// 数组值：+1 表示顺时针，-1 表示逆时针，0 表示非法跳变或无效抖动
static const int8_t ENCODER_STATES[] = {
    0, -1,  1,  0,
    1,  0,  0, -1,
   -1,  0,  0,  1,
    0,  1, -1,  0
};

// 双边沿硬件中断服务函数
void IRAM_ATTR Encoder_ISR() {
    // 高速读取当前两个引脚的物理电平
    uint8_t a = digitalRead(encoder_pin_a);
    uint8_t b = digitalRead(encoder_pin_b);

    // 拼接为 2-bit 的当前状态
    uint8_t current_state = (a << 1) | b;

    // 如果状态发生了实质性改变
    if (current_state != prev_state) {
        // 计算矩阵索引 (4-bit)
        uint8_t index = (prev_state << 2) | current_state;

        // 查表，累加方向步数。抖动产生的正负数会在这里自动完美相消！
        encoder_counter += ENCODER_STATES[index];

        prev_state = current_state;
    }
}

void EncoderSystem::Init(uint8_t pin_a, uint8_t pin_b) {
    encoder_pin_a = pin_a;
    encoder_pin_b = pin_b;
    pinA = pin_a;
    pinB = pin_b;

    pinMode(pinA, INPUT_PULLUP);
    pinMode(pinB, INPUT_PULLUP);

    // 初始化前读取一次基准状态
    uint8_t a = digitalRead(pinA);
    uint8_t b = digitalRead(pinB);
    prev_state = (a << 1) | b;

    // 【关键重构】对 A 相和 B 相同时开启 CHANGE（双边沿）中断
    attachInterrupt(digitalPinToInterrupt(pinA), Encoder_ISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(pinB), Encoder_ISR, CHANGE);
}

int32_t EncoderSystem::GetAndClearDelta() {
    noInterrupts();

    // EC11 编码器每扭动一个物理驻点（咔哒一下），电气状态会完整循环 4 次
    // 因此内部 counter 的变化量是实际物理步数的 4 倍
    // 我们在此处进行安全的除 4 操作，提取完整的物理步数
    int32_t real_clicks = encoder_counter / 4;

    // 巧妙的清零逻辑：只扣除完整的咔哒声，将因停顿在半路或抖动产生的余数保留在池中
    encoder_counter -= (real_clicks * 4);

    interrupts();

    // 取反操作：如果你发现方向反了，直接在这里把 return real_clicks 改为 return -real_clicks 即可
    return real_clicks;
}