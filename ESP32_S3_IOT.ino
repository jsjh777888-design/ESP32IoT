// ============================================================
//  ESP32_S3_IOT.ino — 程式入口與任務調度
//  ESP32-S3 IoT 教學套件
//  溪州網路賣場 · 陳錫昌設計製作
//
//  開發環境：Arduino IDE + arduino-esp32 v3.x（ESP32-S3）
//  Board    ：ESP32S3 Dev Module
//  Flash    ：QIO 80MHz  /  PSRAM：Disabled
//  Port Speed：115200
//
//  ★ WifiManager 使用說明（零全域變數版本）：
//    - WifiManager 不再有全域單例（無 extern wifiMgr）
//    - 由 .ino 持有唯一實例 g_wifi（static，僅此檔案可見）
//    - authManager.begin() 先讀 NVS，再將設定注入 WifiManager
//    - AP 重建旗標由 authManager.apRestartNeeded() 管理，
//      loop() 中輪詢並呼叫 g_wifi.configure() 執行重建
// ============================================================

#include "config.h"
#include "gpio_control.h"
#include "wifi_manager.h"
#include "http_server.h"
#include "button.h"
#include "watchdog.h"
#include "auth_manager.h"

// ── WifiManager 實例（.ino 持有，不對外暴露）────────────────
// ⚠️ 無法在此直接初始化（需要 authManager.begin() 後的值），
//    使用指標，在 setup() 中 new 建立
static WifiManager* g_wifi = nullptr;

// ── Watchdog 餵狗計時 ────────────────────────────────────────
static unsigned long _lastFeedMs = 0;

// ════════════════════════════════════════════════════════════
//  setup()
//
//  初始化順序（順序不可調換）：
//    1. Serial / GPIO
//    2. authManager.begin()   ← 從 NVS 讀取 SSID / 密碼 / Token
//    3. resetBtn.begin()
//    4. g_wifi = new WifiManager(...)  ← 用 NVS 值建立實例
//    5. g_wifi->begin()       ← 啟動 AP
//    6. httpSrv.begin()
//    7. startupSequence()     ← 有 delay(5000)，必須在 wdt 前
//    8. wdt.begin()           ← 最後才啟動 WDT
// ════════════════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n=== ESP32-S3 IoT 教學套件 啟動 ===");
    Serial.println("    溪州網路賣場 · 陳錫昌設計製作");
    Serial.println("===================================");

    // 1. GPIO 初始化
    gpioCtrl.begin();

    // 2. 口令管理器初始化（從 NVS 載入，或套用預設值寫入 NVS）
    //    ★ 必須在 g_wifi 建立之前！
    authManager.begin();

    // 3. 重置按鍵初始化
    resetBtn.begin();

    // 4. 用 NVS 讀到的設定建立 WifiManager 實例
    //    ★ authManager.begin() 之後才能取得正確值
    g_wifi = new WifiManager(
        authManager.getApSsid(),
        authManager.getApPass(),
        authManager.getToken()
    );

    // 5. 啟動 SoftAP
    g_wifi->begin();

    // 6. HTTP Server 啟動
    httpSrv.begin();

    // 7. 上電初始化序列（黃燈亮 5 秒）
    //    ★ 必須在 wdt.begin() 之前！
    gpioCtrl.startupSequence();

    // 8. Watchdog 啟動（上電序列完成後才啟動）
    wdt.begin();

    Serial.println("[Main] 系統就緒，等待手機連線...");
    Serial.printf( "[Main] AP SSID : %s\n",  g_wifi->getSsid().c_str());
    Serial.printf( "[Main] AP IP   : %s\n",  g_wifi->getIp().c_str());
    Serial.printf( "[Main] 主控手機 : %s\n",
                   authManager.hasMaster()
                       ? authManager.getMasterId().c_str()
                       : "(尚無主控)");
}

// ════════════════════════════════════════════════════════════
//  loop()
//
//  執行順序（不可調換）：
//    ① httpSrv.handle()   → 處理 HTTP request，送出 response
//    ② gpioCtrl.update()  → GPIO 自動熄滅計時
//    ③ resetBtn.update()  → 偵測長按重置
//    ④ AP 重建輪詢        → 必須在 ① 之後，確保 response 先送出
//    ⑤ wdt.feed()         → 定時餵狗
// ════════════════════════════════════════════════════════════
void loop() {
    // ① 處理 HTTP 請求（含送出 response）
    httpSrv.handle();

    // ② 更新 GPIO 自動熄滅計時
    gpioCtrl.update();

    // ③ 偵測重置按鍵
    resetBtn.update();
    if (resetBtn.isLongPressed()) {
        // GPIO13 長按 3 秒 → 重設所有設定為預設值
        // resetToDefault() 會清除記憶體、NVS，並設 apRestartNeeded
        authManager.resetToDefault();

        // 三色燈閃爍 3 次作為視覺提示
        for (int i = 0; i < 3; i++) {
            digitalWrite(PIN_GREEN,  HIGH);
            digitalWrite(PIN_YELLOW, HIGH);
            digitalWrite(PIN_RED,    HIGH);
            delay(200);
            digitalWrite(PIN_GREEN,  LOW);
            digitalWrite(PIN_YELLOW, LOW);
            digitalWrite(PIN_RED,    LOW);
            delay(200);
        }
        Serial.println("[Main] 口令已重設為預設值：" DEFAULT_TOKEN);
    }

    // ④ AP 重建輪詢
    //    ★ 必須在 httpSrv.handle() 之後，確保 response 已送出
    //    authManager.applyMasterConfig() / resetToDefault() 會設旗標，
    //    此處偵測到旗標後呼叫 g_wifi->configure() 執行實際重建
    if (authManager.apRestartNeeded()) {
        authManager.clearApRestartFlag();
        g_wifi->configure(
            authManager.getApSsid(),
            authManager.getApPass(),
            authManager.getToken()
        );
    }

    // ⑤ 定時餵狗（每 500ms）
    unsigned long now = millis();
    if (now - _lastFeedMs >= WDT_FEED_MS) {
        wdt.feed();
        _lastFeedMs = now;
    }
}
