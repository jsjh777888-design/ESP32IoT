// ============================================================
//  watchdog.cpp — 硬體看門狗管理
//  arduino-esp32 使用 esp_task_wdt API（同 ESP-IDF v5.x）
// ============================================================
#include "watchdog.h"
#include "config.h"
#include "esp_task_wdt.h"

Watchdog wdt;

void Watchdog::begin() {
    // esp_task_wdt_config_t 為 ESP-IDF v5.x / arduino-esp32 v3.x 新 API
    esp_task_wdt_config_t wdt_cfg = {
        .timeout_ms     = (uint32_t)(WDT_TIMEOUT_S * 1000),
        .idle_core_mask = 0,
        .trigger_panic  = true   // 超時直接 panic 重啟
    };
    esp_task_wdt_reconfigure(&wdt_cfg);
    esp_task_wdt_add(NULL);  // 將目前 task 加入監控
    Serial.printf("[WDT] 看門狗啟動：超時 %d 秒\n", WDT_TIMEOUT_S);
}

void Watchdog::feed() {
    esp_task_wdt_reset();
}
