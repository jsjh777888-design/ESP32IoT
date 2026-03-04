// ============================================================
//  auth_manager.cpp — Token 驗證、主控/從屬手機權限管理
//                     + NVS 持久化儲存
//
//  修正項目對照原始 bug：
//
//  【bug 1】SSID/密碼/口令只能成功設定一次
//    → 原因：applyMasterConfig() 只存記憶體，斷電清空
//    → 修正：applyMasterConfig() 寫 NVS 成功才更新記憶體
//
//  【bug 2】設定完成後重新上電資料亂掉
//    → 原因：begin() 每次都套用 DEFAULT_TOKEN 等常數
//    → 修正：begin() 先呼叫 _loadFromNVS()，有資料才用
//             防斷電：各欄位寫完後最後才寫 NVS_KEY_INITED
//
//  【bug 3】AP 名稱有時沒有更新
//    → 原因：applyMasterConfig() 只改記憶體，未觸發 WiFi 重建
//    → 修正：設定 _apRestartNeeded = true，由 wifi_manager
//             在 loop() 中延遲重建 AP（確保 HTTP response 先送出）
//
//  【bug 4/5】Android 顯示成功但連線失敗、偶爾需重裝
//    → 原因：AP 重建可能在 HTTP response 送出前就執行
//    → 修正：AP 重建完全由 wifi_manager::update() 負責，
//             確保每個 loop() 週期 response 一定先送出
// ============================================================
#include "auth_manager.h"
#include "config.h"

AuthManager authManager;

// ─────────────────────────────────────────────────────────────
AuthManager::AuthManager()
    : _token(DEFAULT_TOKEN),
      _apSsid(AP_SSID),
      _apPass(AP_PASSWORD),
      _masterId(""),
      _hasMaster(false),
      _apRestartNeeded(false)
{}

// ── begin() ──────────────────────────────────────────────────
// 優先從 NVS 載入；NVS 空白才套用預設值並寫入 NVS
// ⚠️ 不呼叫任何 WiFi.xxx()，AP 由 wifi_manager 負責
void AuthManager::begin() {
    Serial.println("[Auth] 初始化開始");

    if (_loadFromNVS()) {
        Serial.printf("[Auth] NVS 載入成功：SSID=%s  Token長度=%d  Master=%s\n",
                      _apSsid.c_str(),
                      _token.length(),
                      _hasMaster ? _masterId.c_str() : "(無)");
    } else {
        Serial.println("[Auth] NVS 無有效資料，套用出廠預設值");
        _token    = DEFAULT_TOKEN;
        _apSsid   = AP_SSID;
        _apPass   = AP_PASSWORD;
        _masterId = "";
        _hasMaster = false;

        if (_saveToNVS()) {
            Serial.println("[Auth] 預設值已寫入 NVS");
        } else {
            Serial.println("[Auth] ⚠️ 預設值寫入 NVS 失敗");
        }
    }
    Serial.println("[Auth] 初始化完成");
}

// ── _isDefault() ─────────────────────────────────────────────
bool AuthManager::_isDefault() const {
    return (_token  == DEFAULT_TOKEN) &&
           (_apSsid == AP_SSID)       &&
           (_apPass == AP_PASSWORD);
}

// ── Getters ──────────────────────────────────────────────────
bool   AuthManager::validateToken(const String &t) const { return (t.length() > 0) && (t == _token); }
String AuthManager::getToken()    const { return _token;    }
String AuthManager::getApSsid()   const { return _apSsid;   }
String AuthManager::getApPass()   const { return _apPass;   }
bool   AuthManager::hasMaster()   const { return _hasMaster;}
String AuthManager::getMasterId() const { return _masterId; }
bool   AuthManager::apRestartNeeded() const { return _apRestartNeeded; }
void   AuthManager::clearApRestartFlag()    { _apRestartNeeded = false; }

// ── resetToDefault() ─────────────────────────────────────────
// GPIO13 長按 3 秒觸發：清除記憶體 + 清除 NVS + 標記 AP 重建
void AuthManager::resetToDefault() {
    _token     = DEFAULT_TOKEN;
    _apSsid    = AP_SSID;
    _apPass    = AP_PASSWORD;
    _masterId  = "";
    _hasMaster = false;

    // 清除 NVS
    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, false)) {
        prefs.clear();
        prefs.end();
        Serial.println("[Auth] NVS 已清除");
    } else {
        prefs.end();
        Serial.println("[Auth] ⚠️ NVS clear 失敗");
    }

    // 標記 AP 需重建（由 wifi_manager loop() 延遲執行）
    _apRestartNeeded = true;
    Serial.println("[Auth] 已重設為預設值，主控手機清除，AP 重建待執行");
}

// ── evaluate() ───────────────────────────────────────────────
AuthResult AuthManager::evaluate(const String &ssid,
                                  const String &pass,
                                  const String &token) const {
    AuthResult result;
    bool ssidMatch  = (ssid  == _apSsid);
    bool passMatch  = (pass  == _apPass);
    bool tokenMatch = (token == _token);

    result.tokenOk  = tokenMatch;
    result.isMaster = false;  // 由呼叫端比對 clientId 後設定

    if (ssidMatch && passMatch && tokenMatch) {
        result.role       = ClientRole::SLAVE_OK;
        result.canOperate = true;
        result.canConfig  = true;
    } else if (!ssidMatch && !passMatch && !tokenMatch) {
        result.role       = ClientRole::DENIED;
        result.canOperate = false;
        result.canConfig  = false;
    } else {
        result.role       = ClientRole::SLAVE_DIFF;
        result.canOperate = false;
        result.canConfig  = true;
    }
    return result;
}

// ── claimMaster() ────────────────────────────────────────────
bool AuthManager::claimMaster(const String &clientId) {
    if (_hasMaster) {
        return (_masterId == clientId);
    }
    _hasMaster = true;
    _masterId  = clientId;

    // 單獨持久化 master_id
    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, false)) {
        prefs.putString(NVS_KEY_MASTER_ID, clientId);
        prefs.end();
    } else {
        prefs.end();
        Serial.println("[Auth] ⚠️ claimMaster：NVS 寫入失敗（記憶體有效，斷電後清除）");
    }

    Serial.printf("[Auth] ✅ 主控手機已登記：%s\n", clientId.c_str());
    return true;
}

// ── applyMasterConfig() ──────────────────────────────────────
// 修正核心：先驗證→先存 NVS→NVS 成功才更新記憶體→標記 AP 重建
bool AuthManager::applyMasterConfig(const String &newSsid,
                                     const String &newPass,
                                     const String &newToken) {
    // 參數驗證
    if (newSsid.isEmpty() || newSsid.length() > 32) {
        Serial.println("[Auth] applyMasterConfig: SSID 長度不合法（1~32 字元）");
        return false;
    }
    if (!newPass.isEmpty() && (newPass.length() < 8 || newPass.length() > 64)) {
        Serial.println("[Auth] applyMasterConfig: 密碼長度不合法（8~64 字元）");
        return false;
    }
    if (newToken.isEmpty() || newToken.length() > TOKEN_MAX_LEN) {
        Serial.println("[Auth] applyMasterConfig: Token 長度不合法");
        return false;
    }

    // 暫存舊值，用於 NVS 失敗時回滾
    String oldSsid  = _apSsid;
    String oldPass  = _apPass;
    String oldToken = _token;

    // 先寫入候選值到 NVS（用暫時變數，不影響現有狀態）
    _apSsid = newSsid;
    _apPass = newPass;
    _token  = newToken;

    if (!_saveToNVS()) {
        // NVS 失敗：回滾記憶體，拒絕本次更新
        Serial.println("[Auth] ⚠️ NVS 寫入失敗，設定回滾");
        _apSsid = oldSsid;
        _apPass = oldPass;
        _token  = oldToken;
        return false;
    }

    // 若恢復預設值 → 清除主控手機
    if (_isDefault()) {
        _hasMaster = false;
        _masterId  = "";
        Serial.println("[Auth] 設定恢復預設值，主控手機已清除");
    }

    // 標記 AP 需重建（由 wifi_manager loop() 延遲執行）
    _apRestartNeeded = true;

    Serial.printf("[Auth] ✅ 設定更新完成：SSID=%s  Token長度=%d\n",
                  _apSsid.c_str(), _token.length());
    return true;
}

// =============================================================================
// 私有 NVS 方法
// =============================================================================

// ── _loadFromNVS() ───────────────────────────────────────────
bool AuthManager::_loadFromNVS() {
    Preferences prefs;

    if (!prefs.begin(NVS_NAMESPACE, true)) {  // true = 唯讀
        Serial.println("[Auth] NVS begin(readonly) 失敗");
        prefs.end();
        return false;
    }

    // 確認 inited 旗標（防斷電保護）
    if (!prefs.getBool(NVS_KEY_INITED, false)) {
        prefs.end();
        return false;  // 從未完整寫入過
    }

    _apSsid    = prefs.getString(NVS_KEY_SSID,      AP_SSID);
    _apPass    = prefs.getString(NVS_KEY_PASSWORD,   AP_PASSWORD);
    _token     = prefs.getString(NVS_KEY_TOKEN,      DEFAULT_TOKEN);
    _masterId  = prefs.getString(NVS_KEY_MASTER_ID,  "");
    _hasMaster = !_masterId.isEmpty();

    prefs.end();  // ⚠️ 必須 end()
    return true;
}

// ── _saveToNVS() ─────────────────────────────────────────────
// 防斷電策略：各欄位依序寫入 → 最後才寫 NVS_KEY_INITED = true
// 若中途斷電，inited 未設 → 下次開機自動回預設值（不會讀到損壞資料）
bool AuthManager::_saveToNVS() {
    Preferences prefs;

    if (!prefs.begin(NVS_NAMESPACE, false)) {  // false = 讀寫
        Serial.println("[Auth] ⚠️ NVS begin(write) 失敗");
        prefs.end();
        return false;
    }

    bool ok = true;
    ok &= (prefs.putString(NVS_KEY_SSID,      _apSsid) > 0);
    ok &= (prefs.putString(NVS_KEY_PASSWORD,  _apPass)  > 0);
    ok &= (prefs.putString(NVS_KEY_TOKEN,     _token)   > 0);
    prefs.putString(NVS_KEY_MASTER_ID, _masterId);  // 空字串合法

    if (ok) {
        // 所有重要欄位成功後，最後才標記 inited
        ok &= prefs.putBool(NVS_KEY_INITED, true);
    }

    prefs.end();  // ⚠️ 必須 end()，確保 flush 到 Flash

    if (!ok) Serial.println("[Auth] ⚠️ NVS 部分欄位寫入失敗");
    return ok;
}
