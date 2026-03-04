#pragma once
// ============================================================
//  auth_manager.h — Token 驗證、主控/從屬手機權限管理
//                   + NVS 持久化儲存
//
//  ★ 修正重點：
//    1. begin() 改為從 NVS 載入，而非每次都回預設值
//    2. applyMasterConfig() 先寫 NVS 成功才更新記憶體
//    3. resetToDefault() 同步清除 NVS
//    4. 防斷電：最後才寫 NVS_KEY_INITED，確保資料完整性
//    5. 新增 _apRestartNeeded 旗標供 wifi_manager 使用
// ============================================================
#include <Arduino.h>
#include <Preferences.h>

// ── 手機角色定義 ─────────────────────────────────────────────
enum class ClientRole {
    UNKNOWN,    // 尚未驗證
    MASTER,     // 主控手機（第一台修改預設值的手機）
    SLAVE_OK,   // 從屬手機（設定與 ESP32 相同，綠燈）
    SLAVE_DIFF, // 從屬手機（設定與 ESP32 不同，黃燈）
    DENIED      // 設定完全不符，禁止操作（紅燈）
};

// ── 驗證請求結果 ─────────────────────────────────────────────
struct AuthResult {
    bool        tokenOk;    // 口令是否正確
    ClientRole  role;       // 手機角色
    bool        canOperate; // 是否允許 A/B/C/D 操作
    bool        canConfig;  // 是否允許修改設定
    bool        isMaster;   // 是否為主控手機
};

class AuthManager {
public:
    AuthManager();

    // ── 初始化（從 NVS 載入；NVS 空白則套用預設值並寫入）──
    void begin();

    // ── 口令驗證 ──────────────────────────────────────────────
    bool validateToken(const String &token) const;

    // ── 取得目前設定值 ────────────────────────────────────────
    String getToken()  const;
    String getApSsid() const;
    String getApPass() const;

    // ── 重設為預設值（GPIO13 長按觸發）──────────────────────
    // 同步清除 NVS，下次開機仍為預設值
    void resetToDefault();

    // ── 判斷手機角色與操作權限 ───────────────────────────────
    AuthResult evaluate(const String &ssid,
                        const String &pass,
                        const String &token) const;

    // ── 主控手機更新 ESP32 設定（含 NVS 持久化）─────────────
    // 回傳 false = 參數不合法 或 NVS 寫入失敗
    bool applyMasterConfig(const String &newSsid,
                           const String &newPass,
                           const String &newToken);

    // ── 主控手機查詢 ─────────────────────────────────────────
    bool   hasMaster()   const;
    String getMasterId() const;

    // ── 宣告主控手機 ─────────────────────────────────────────
    // 回傳 true = 成功取得主控權；false = 已有其他主控
    bool claimMaster(const String &clientId);

    // ── AP 重建旗標（applyMasterConfig / resetToDefault 後設 true）
    // wifi_manager 在 loop() 中檢查此旗標，確保 HTTP response
    // 已送出後才重建 AP
    bool apRestartNeeded() const;
    void clearApRestartFlag();

private:
    String _token;
    String _apSsid;
    String _apPass;
    String _masterId;
    bool   _hasMaster;
    bool   _apRestartNeeded;  // AP 重建待執行旗標

    bool _loadFromNVS();   // 回傳 false = NVS 空白或損壞
    bool _saveToNVS();     // 回傳 false = 寫入失敗
    bool _isDefault() const;
};

extern AuthManager authManager;
