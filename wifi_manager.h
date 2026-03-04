#pragma once
// ============================================================
//  wifi_manager.h — Wi-Fi SoftAP 設定模組
//
//  設計原則：
//    - 完全 class 封裝，零全域變數
//    - 所有設定存於 instance 內部（_ssid / _password / ...）
//    - 外部透過 getter 取得狀態，不直接讀 config.h 常數
//    - AP 重建三步驟封裝於 _applyAP()（私有）
//    - 呼叫端持有 WifiManager 物件，不依賴 extern 單例
//
//  典型用法：
//    WifiManager wifi("MySSID", "MyPass", "token123");
//    wifi.begin();
//
//    // 更新設定（NVS 寫入由呼叫端決定時機）
//    if (wifi.configure("NewSSID", "NewPass", "NewToken")) {
//        // AP 已自動重建，response 送出後才呼叫此函式
//    }
// ============================================================
#include <Arduino.h>
#include <WiFi.h>

class WifiManager {
public:
    // ── 建構子 ────────────────────────────────────────────────
    // 所有設定在建構時注入，不依賴任何全域常數
    // channel   預設 1，一般不需修改
    // maxConn   預設 4
    explicit WifiManager(const String &ssid,
                         const String &password,
                         const String &token,
                         uint8_t       channel = 1,
                         uint8_t       maxConn = 4);

    // ── 啟動 SoftAP ───────────────────────────────────────────
    // 使用建構時注入的設定啟動 AP
    // 回傳 true = 成功，false = WiFi.softAP() 失敗
    bool begin();

    // ── 更新設定並重建 AP ─────────────────────────────────────
    // 將 instance 內部設定替換為新值，並立即重建 AP
    // ⚠️ 呼叫前必須確保 HTTP response 已送出
    // 回傳 true = AP 重建成功，false = 參數不合法或重建失敗
    bool configure(const String &newSsid,
                   const String &newPassword,
                   const String &newToken);

    // ── Getters（唯讀）────────────────────────────────────────
    String  getSsid()     const;
    String  getPassword() const;
    String  getToken()    const;
    String  getIp()       const;   // softAPIP 字串
    uint8_t getChannel()  const;
    uint8_t getMaxConn()  const;
    bool    isRunning()   const;   // AP 是否已啟動

    // ── 除錯輸出 ──────────────────────────────────────────────
    void printInfo() const;

private:
    // ── 設定值（instance 內部持有）────────────────────────────
    String  _ssid;
    String  _password;
    String  _token;
    uint8_t _channel;
    uint8_t _maxConn;
    bool    _running;   // AP 目前是否在線

    // ── 私有：執行 AP 重建三步驟 ──────────────────────────────
    // disconnect → mode → softAP
    // 回傳 true = softAP() 成功
    bool _applyAP();

    // ── 私有：參數驗證 ────────────────────────────────────────
    // 回傳 false = 任一欄位不合法（會印 Serial 錯誤訊息）
    bool _validate(const String &ssid,
                   const String &password,
                   const String &token) const;
};
