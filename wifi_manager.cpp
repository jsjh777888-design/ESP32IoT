// ============================================================
//  wifi_manager.cpp — Wi-Fi SoftAP 設定模組
//
//  重構說明（對比舊版）：
//
//  舊版問題：
//    1. `WifiManager wifiMgr;` 全域單例，任何地方都能直接存取
//    2. begin() 直接讀 AP_SSID / AP_PASSWORD 等 config.h 常數
//       → 設定與類別耦合，無法在執行期注入不同設定
//    3. 無狀態：不知道 AP 是否已啟動
//    4. 無參數驗證
//    5. 無 AP 重建流程（只有初始化）
//
//  新版設計：
//    1. 零全域變數：呼叫端持有物件，完整控制生命週期
//    2. 建構子注入：ssid / password / token 從外部傳入
//    3. _running 旗標追蹤 AP 狀態
//    4. _validate() 在 begin() / configure() 前檢查參數合法性
//    5. _applyAP() 封裝三步驟重建（disconnect→mode→softAP）
//    6. configure() 先驗證、再更新、再重建，失敗時不改動狀態
// ============================================================
#include "wifi_manager.h"

// ── AP 重建時序常數（ms）────────────────────────────────────
// 不依賴 config.h，讓模組可獨立使用
static constexpr uint32_t kDisconnectDelayMs = 500;
static constexpr uint32_t kModeDelayMs       = 300;

// ════════════════════════════════════════════════════════════
//  建構子
// ════════════════════════════════════════════════════════════
WifiManager::WifiManager(const String &ssid,
                         const String &password,
                         const String &token,
                         uint8_t       channel,
                         uint8_t       maxConn)
    : _ssid(ssid),
      _password(password),
      _token(token),
      _channel(channel),
      _maxConn(maxConn),
      _running(false)
{}

// ════════════════════════════════════════════════════════════
//  begin() — 首次啟動 AP
// ════════════════════════════════════════════════════════════
bool WifiManager::begin() {
    Serial.println("[WiFi] begin() 啟動 SoftAP");

    if (!_validate(_ssid, _password, _token)) {
        return false;
    }

    return _applyAP();
}

// ════════════════════════════════════════════════════════════
//  configure() — 更新設定並重建 AP
//
//  ⚠️ 呼叫前必須確保 HTTP response 已送出
//     本函式會執行 WiFi.softAPdisconnect()，
//     若在 handler 內直接呼叫，response 可能送不出去
// ════════════════════════════════════════════════════════════
bool WifiManager::configure(const String &newSsid,
                             const String &newPassword,
                             const String &newToken) {
    // 先驗證，失敗則完全不改動現有狀態
    if (!_validate(newSsid, newPassword, newToken)) {
        return false;
    }

    // 暫存舊值，_applyAP() 失敗時可回滾
    const String oldSsid     = _ssid;
    const String oldPassword = _password;
    const String oldToken    = _token;

    // 更新 instance 內部狀態
    _ssid     = newSsid;
    _password = newPassword;
    _token    = newToken;

    // 重建 AP
    if (!_applyAP()) {
        // 重建失敗：回滾設定，維持舊的 AP 狀態
        _ssid     = oldSsid;
        _password = oldPassword;
        _token    = oldToken;
        Serial.println("[WiFi] ⚠️ configure() AP 重建失敗，設定回滾");
        return false;
    }

    return true;
}

// ════════════════════════════════════════════════════════════
//  Getters
// ════════════════════════════════════════════════════════════
String  WifiManager::getSsid()     const { return _ssid;     }
String  WifiManager::getPassword() const { return _password; }
String  WifiManager::getToken()    const { return _token;    }
String  WifiManager::getIp()       const { return WiFi.softAPIP().toString(); }
uint8_t WifiManager::getChannel()  const { return _channel;  }
uint8_t WifiManager::getMaxConn()  const { return _maxConn;  }
bool    WifiManager::isRunning()   const { return _running;  }

// ════════════════════════════════════════════════════════════
//  printInfo()
// ════════════════════════════════════════════════════════════
void WifiManager::printInfo() const {
    Serial.println("[WiFi] ── AP 狀態 ──────────────");
    Serial.printf( "[WiFi] SSID     : %s\n",  _ssid.c_str());
    Serial.printf( "[WiFi] Password : (%d chars)\n", _password.length());
    Serial.printf( "[WiFi] Token    : (%d chars)\n", _token.length());
    Serial.printf( "[WiFi] IP       : %s\n",  WiFi.softAPIP().toString().c_str());
    Serial.printf( "[WiFi] Channel  : %d\n",  _channel);
    Serial.printf( "[WiFi] Max Conn : %d\n",  _maxConn);
    Serial.printf( "[WiFi] Running  : %s\n",  _running ? "yes" : "no");
    Serial.println("[WiFi] ────────────────────────");
}

// ════════════════════════════════════════════════════════════
//  私有：_applyAP()
//  AP 重建三步驟（所有啟動/重建路徑的唯一入口）
// ════════════════════════════════════════════════════════════
bool WifiManager::_applyAP() {
    Serial.printf("[WiFi] _applyAP() SSID=%s CH=%d\n",
                  _ssid.c_str(), _channel);

    // 步驟 1：斷開所有連線，停止舊 AP
    _running = false;
    WiFi.softAPdisconnect(true);
    delay(kDisconnectDelayMs);

    // 步驟 2：設定 AP 模式，等待 stack 穩定
    WiFi.mode(WIFI_AP);
    delay(kModeDelayMs);

    // 步驟 3：用 instance 內部設定啟動 AP
    const bool ok = WiFi.softAP(
        _ssid.c_str(),
        _password.c_str(),
        _channel,
        0,         // ssid_hidden: 0 = 廣播
        _maxConn
    );

    _running = ok;

    if (ok) {
        Serial.printf("[WiFi] ✅ AP 啟動成功  IP=%s\n",
                      WiFi.softAPIP().toString().c_str());
    } else {
        Serial.println("[WiFi] ❌ WiFi.softAP() 回傳失敗");
    }

    return ok;
}

// ════════════════════════════════════════════════════════════
//  私有：_validate()
//  在任何設定生效前做基本邊界檢查
// ════════════════════════════════════════════════════════════
bool WifiManager::_validate(const String &ssid,
                             const String &password,
                             const String &token) const {
    // SSID：2 ~ 32 字元（IEEE 802.11 規範）
    if (ssid.length() < 2 || ssid.length() > 32) {
        Serial.printf("[WiFi] 驗證失敗：SSID 長度 %d（需 2~32）\n",
                      ssid.length());
        return false;
    }

    // 密碼：空字串（Open AP）或 8~63 字元（WPA2-PSK）
    if (password.length() > 0 &&
        (password.length() < 8 || password.length() > 63)) {
        Serial.printf("[WiFi] 驗證失敗：密碼長度 %d（需 0 或 8~63）\n",
                      password.length());
        return false;
    }

    // Token：1 ~ 64 字元
    if (token.isEmpty() || token.length() > 64) {
        Serial.printf("[WiFi] 驗證失敗：Token 長度 %d（需 1~64）\n",
                      token.length());
        return false;
    }

    return true;
}
