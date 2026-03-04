#pragma once
// ============================================================
//  watchdog.h — 硬體看門狗管理
//  ★ 新增 header（原來缺少此檔案）
//  arduino-esp32 v3.x 使用 esp_task_wdt_reconfigure API
// ============================================================
#include <Arduino.h>

class Watchdog {
public:
    void begin();  // 啟動 WDT，超時後 panic 重啟
    void feed();   // 餵狗（重置計時器）
};

extern Watchdog wdt;
