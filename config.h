#pragma once
// ============================================================
//  config.h — 系統常數定義
//  ESP32-S3 IoT 教學套件
//  溪州網路賣場 · 陳錫昌設計製作
//  開發環境：Arduino IDE + arduino-esp32 v3.x（ESP32-S3）
// ============================================================

// ── GPIO 腳位 ────────────────────────────────────────────────
#define PIN_GREEN       35    // 輸出：綠燈
#define PIN_YELLOW      36    // 輸出：黃燈
#define PIN_RED         37    // 輸出：紅燈
#define PIN_RESET_BTN   13    // 輸入：重置按鍵（低電位觸發，內建上拉）

// ── Wi-Fi AP 出廠預設值 ──────────────────────────────────────
#define AP_SSID         "ESP32-IoT-Lab"
#define AP_PASSWORD     "12345678"
#define AP_CHANNEL      1
#define AP_MAX_CONN     4     // 最多同時連線台數

// ── 預設口令 ─────────────────────────────────────────────────
#define DEFAULT_TOKEN   "iot_token_123"
#define TOKEN_MAX_LEN   64

// ── NVS Namespace & Keys ─────────────────────────────────────
// ⚠️ namespace 最長 15 字元 / key 最長 15 字元（NVS 硬性限制）
#define NVS_NAMESPACE       "iot-cfg"       // 15 字元以內
#define NVS_KEY_SSID        "ssid"
#define NVS_KEY_PASSWORD    "appass"
#define NVS_KEY_TOKEN       "token"
#define NVS_KEY_MASTER_ID   "master_id"
#define NVS_KEY_INITED      "inited"        // 防斷電完整性旗標

// ── 時序設定（毫秒）─────────────────────────────────────────
#define STARTUP_YELLOW_MS   5000  // 上電黃燈亮持續時間
#define GPIO_PULSE_MS       1000  // A/B/C 指令 GPIO 高電位持續時間
#define BTN_DEBOUNCE_MS     50    // 按鍵消抖時間
#define BTN_LONG_PRESS_MS   3000  // 長按判定時間（重設口令）
#define BTN_RELOCK_MS       2000  // 重設後防重複觸發鎖定時間
#define WDT_TIMEOUT_S       5     // Watchdog 超時（秒）
#define WDT_FEED_MS         500   // Watchdog 餵狗間隔

// ── AP 重建時序 ───────────────────────────────────────────────
#define AP_DISCONNECT_DELAY_MS  500   // softAPdisconnect 後等待
#define AP_RESTART_DELAY_MS     300   // WiFi.mode() 後等待

// ── HTTP Server ──────────────────────────────────────────────
#define HTTP_PORT       80

// ── 主控/從屬手機邏輯說明 ────────────────────────────────────
//   第一台在預設口令下修改任何設定的手機，成為「主控手機」
//   主控手機更改設定 → 同步更新 ESP32 設定值（含 NVS 持久化）
//   從屬手機（設定與 ESP32 相同）→ 綠燈，可操作
//   從屬手機（設定與 ESP32 不同）→ 黃燈，僅能改自身設定
//   任何手機（設定與 ESP32 完全不同且非主控）→ 紅燈，禁止操作
//   所有設定恢復預設值 → 清除主控手機，恢復平等狀態
