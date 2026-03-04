#pragma once
// ============================================================
//  gpio_control.h — GPIO 燈號控制與即時狀態管理
// ============================================================
#include <Arduino.h>

class GpioControl {
public:
    GpioControl();
    void begin();           // 初始化 GPIO 腳位
    void startupSequence(); // 上電序列：黃燈亮 5 秒後熄滅

    // ── 指令執行 ──────────────────────────────────────────────
    void cmdA();  // 綠燈亮 1 秒（GPIO35 pulse）
    void cmdB();  // 黃燈亮 1 秒，同時取消 D 長亮
    void cmdC();  // 紅燈亮 1 秒（GPIO37 pulse）
    void cmdD();  // 黃燈長亮（維持到 B 指令）

    // ── loop() 中呼叫，處理自動熄滅計時 ─────────────────────
    void update();

    // ── 即時狀態查詢（供 /status API 使用）──────────────────
    int  getGreen()       const;
    int  getYellow()      const;
    int  getRed()         const;
    bool getYellowHold()  const;

private:
    // 自動熄滅計時器
    unsigned long _greenOffTime;   // 綠燈熄滅時間（millis）
    unsigned long _yellowOffTime;  // 黃燈熄滅時間（0 = 不熄滅）
    unsigned long _redOffTime;     // 紅燈熄滅時間

    bool _yellowHold;  // D 指令長亮旗標

    // 即時電位狀態
    int _green;
    int _yellow;
    int _red;

    void _pulseGreen(unsigned long ms);
    void _pulseYellow(unsigned long ms);
    void _pulseRed(unsigned long ms);
};

extern GpioControl gpioCtrl;
