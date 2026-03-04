// ============================================================
//  http_server.cpp — HTTP API 實作
//
//  ★ 修正：/config 端點呼叫 applyMasterConfig() 後
//           不直接重建 AP，改由 authManager._apRestartNeeded
//           旗標通知 wifi_manager 在 loop() 延遲執行
//
//  端點一覽：
//    GET /control  ?cmd=A|B|C|D&token=&ssid=&pass=
//    GET /status   （無需口令，回傳三燈 JSON）
//    GET /config   ?ssid=&pass=&token=&newSsid=&newPass=&newToken=&clientId=
//    GET /role     ?ssid=&pass=&token=&clientId=
// ============================================================
#include "http_server.h"
#include "config.h"
#include "auth_manager.h"
#include "gpio_control.h"

HttpServer httpSrv;

HttpServer::HttpServer() : _server(HTTP_PORT) {}

void HttpServer::begin() {
    _server.on("/control",  [this]() { _handleControl();  });
    _server.on("/status",   [this]() { _handleStatus();   });
    _server.on("/config",   [this]() { _handleConfig();   });
    _server.on("/role",     [this]() { _handleRole();     });
    _server.onNotFound(     [this]() { _handleNotFound(); });
    _server.begin();
    Serial.printf("[HTTP] Server 啟動 port %d\n", HTTP_PORT);
}

void HttpServer::handle() {
    _server.handleClient();
}

// ── Helper ───────────────────────────────────────────────────
void HttpServer::_sendJson(int code, const String &json) {
    _server.sendHeader("Access-Control-Allow-Origin", "*");
    _server.send(code, "application/json", json);
}

String HttpServer::_clientId() {
    return _server.client().remoteIP().toString();
}

// ════════════════════════════════════════════════════════════
//  GET /control?cmd=A|B|C|D&token=<口令>[&ssid=<>&pass=<>]
// ════════════════════════════════════════════════════════════
void HttpServer::_handleControl() {
    String cmd   = _server.arg("cmd");
    String token = _server.arg("token");
    String ssid  = _server.arg("ssid");
    String pass  = _server.arg("pass");

    // ssid/pass 未傳時，預設使用 ESP32 目前值（方便單純操作）
    if (ssid.isEmpty()) ssid = authManager.getApSsid();
    if (pass.isEmpty()) pass = authManager.getApPass();

    AuthResult auth = authManager.evaluate(ssid, pass, token);

    if (!auth.tokenOk) {
        _sendJson(401, "{\"error\":\"invalid_token\"}");
        Serial.printf("[HTTP] /control 口令錯誤 token=%s\n", token.c_str());
        return;
    }
    if (!auth.canOperate) {
        _sendJson(403, "{\"error\":\"not_authorized\","
                       "\"reason\":\"yellow_or_red_light\"}");
        Serial.println("[HTTP] /control 拒絕：黃燈或紅燈狀態");
        return;
    }

    String result = "ok";
    if      (cmd == "A") { gpioCtrl.cmdA(); }
    else if (cmd == "B") { gpioCtrl.cmdB(); }
    else if (cmd == "C") { gpioCtrl.cmdC(); }
    else if (cmd == "D") { gpioCtrl.cmdD(); }
    else                 { result = "unknown_cmd"; }

    _sendJson(200, "{\"cmd\":\"" + cmd + "\","
                   "\"result\":\"" + result + "\"}");
}

// ════════════════════════════════════════════════════════════
//  GET /status — 無需口令，回傳三燈即時狀態
// ════════════════════════════════════════════════════════════
void HttpServer::_handleStatus() {
    String resp = "{";
    resp += "\"green\":"       + String(gpioCtrl.getGreen())              + ",";
    resp += "\"yellow\":"      + String(gpioCtrl.getYellow())             + ",";
    resp += "\"red\":"         + String(gpioCtrl.getRed())                + ",";
    resp += "\"yellow_hold\":" + String(gpioCtrl.getYellowHold() ? 1 : 0);
    resp += "}";
    _sendJson(200, resp);
}

// ════════════════════════════════════════════════════════════
//  GET /config?ssid=<>&pass=<>&token=<>
//             &newSsid=<>&newPass=<>&newToken=<>&clientId=<ip>
//
//  ★ 修正：applyMasterConfig() 成功後不直接重建 AP
//           AP 重建由 authManager._apRestartNeeded 旗標
//           通知 wifi_manager 在 loop() 延遲執行，
//           確保 HTTP response 先到達 Android
// ════════════════════════════════════════════════════════════
void HttpServer::_handleConfig() {
    String ssid     = _server.arg("ssid");
    String pass     = _server.arg("pass");
    String token    = _server.arg("token");
    String newSsid  = _server.arg("newSsid");
    String newPass  = _server.arg("newPass");
    String newToken = _server.arg("newToken");
    String clientId = _server.arg("clientId");

    if (clientId.isEmpty()) clientId = _clientId();

    // 驗證角色
    AuthResult auth = authManager.evaluate(ssid, pass, token);

    // 紅燈：完全拒絕
    if (auth.role == ClientRole::DENIED) {
        _sendJson(403, "{\"error\":\"denied\",\"reason\":\"red_light\"}");
        return;
    }

    // 黃燈：從屬手機只能改自身（不更新 ESP32）
    if (auth.role == ClientRole::SLAVE_DIFF) {
        _sendJson(200, "{\"result\":\"local_only\","
                       "\"reason\":\"yellow_light_slave_only\"}");
        Serial.println("[HTTP] /config 黃燈從屬，僅限改自身設定");
        return;
    }

    // 綠燈：嘗試成為主控手機（若尚無主控且確實有修改）
    bool isMasterNow = false;
    if (!authManager.hasMaster()) {
        bool changing =
            (!newSsid.isEmpty()  && newSsid  != authManager.getApSsid()) ||
            (!newPass.isEmpty()  && newPass  != authManager.getApPass()) ||
            (!newToken.isEmpty() && newToken != authManager.getToken());
        if (changing) {
            authManager.claimMaster(clientId);
            isMasterNow = true;
        }
    } else {
        // 已有主控：只有主控本身才能更新
        isMasterNow = (authManager.getMasterId() == clientId);
        if (!isMasterNow) {
            _sendJson(403, "{\"error\":\"master_exists\","
                           "\"master_id\":\"" + authManager.getMasterId() + "\"}");
            return;
        }
    }

    // 合併新舊設定（空字串 = 沿用現有值）
    String applySsid  = newSsid.isEmpty()  ? authManager.getApSsid() : newSsid;
    String applyPass  = newPass.isEmpty()  ? authManager.getApPass() : newPass;
    String applyToken = newToken.isEmpty() ? authManager.getToken()  : newToken;

    // ★ 修正核心：applyMasterConfig() 內部會：
    //    1. 驗證參數合法性
    //    2. 寫入 NVS（成功才更新記憶體）
    //    3. 設定 _apRestartNeeded = true
    //    AP 重建由 wifiMgr.update() 在下一個 loop() 執行
    bool ok = authManager.applyMasterConfig(applySsid, applyPass, applyToken);

    if (!ok) {
        _sendJson(500, "{\"error\":\"apply_failed\","
                       "\"reason\":\"nv_write_error_or_invalid_params\"}");
        return;
    }

    String resp = "{\"result\":\"ok\",";
    resp += "\"is_master\":"   + String(isMasterNow                   ? "true" : "false") + ",";
    resp += "\"has_master\":"  + String(authManager.hasMaster()       ? "true" : "false") + ",";
    resp += "\"current_ssid\":\"" + authManager.getApSsid()           + "\",";
    resp += "\"ap_restart\":\"pending\"}";  // 告知 Android AP 正在重建
    _sendJson(200, resp);
}

// ════════════════════════════════════════════════════════════
//  GET /role?ssid=<>&pass=<>&token=<>&clientId=<ip>
//  Android App 用此端點更新確認鍵 1/2 燈號顯示
// ════════════════════════════════════════════════════════════
void HttpServer::_handleRole() {
    String ssid     = _server.arg("ssid");
    String pass     = _server.arg("pass");
    String token    = _server.arg("token");
    String clientId = _server.arg("clientId");

    if (clientId.isEmpty()) clientId = _clientId();

    AuthResult auth = authManager.evaluate(ssid, pass, token);

    bool ssidMatch  = (ssid == authManager.getApSsid() &&
                       pass == authManager.getApPass());
    bool tokenMatch = (token == authManager.getToken());

    String key1Light = ssidMatch  ? "green" : (tokenMatch  ? "yellow" : "red");
    String key2Light = tokenMatch ? "green" : (ssidMatch   ? "yellow" : "red");

    bool isMaster = authManager.hasMaster() &&
                    (authManager.getMasterId() == clientId);

    String roleStr;
    switch (auth.role) {
        case ClientRole::SLAVE_OK:   roleStr = "slave_ok";   break;
        case ClientRole::SLAVE_DIFF: roleStr = "slave_diff"; break;
        case ClientRole::DENIED:     roleStr = "denied";     break;
        default:                     roleStr = "unknown";    break;
    }
    if (isMaster) roleStr = "master";

    String resp = "{";
    resp += "\"role\":\""       + roleStr    + "\",";
    resp += "\"key1_light\":\"" + key1Light  + "\",";
    resp += "\"key2_light\":\"" + key2Light  + "\",";
    resp += "\"can_operate\":"  + String(auth.canOperate ? "true" : "false") + ",";
    resp += "\"can_config\":"   + String(auth.canConfig  ? "true" : "false") + ",";
    resp += "\"is_master\":"    + String(isMaster        ? "true" : "false") + ",";
    resp += "\"has_master\":"   + String(authManager.hasMaster() ? "true" : "false");
    resp += "}";
    _sendJson(200, resp);
}

void HttpServer::_handleNotFound() {
    _sendJson(404, "{\"error\":\"not_found\"}");
}
