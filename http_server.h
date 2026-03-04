#pragma once
// ============================================================
//  http_server.h — HTTP API 實作（WebServer）
//
//  端點：
//    GET /control  ?cmd=A|B|C|D&token=<口令>
//                  &ssid=<ssid>&pass=<pass>          (設定更新用)
//    GET /status   無需口令，回傳三燈 JSON 狀態
//    GET /config   ?ssid=<>&pass=<>&token=<>&newToken=<>
//                  主控/從屬手機更新設定
//    GET /role     ?ssid=<>&pass=<>&token=<>&clientId=<ip>
//                  查詢手機角色（綠/黃/紅燈判斷）
// ============================================================
#include <Arduino.h>
#include <WebServer.h>

class HttpServer {
public:
    HttpServer();
    void begin();   // 啟動 HTTP Server（port 80）
    void handle();  // 在 loop() 中呼叫
private:
    WebServer _server;

    void _handleControl();  // /control
    void _handleStatus();   // /status
    void _handleConfig();   // /config
    void _handleRole();     // /role
    void _handleNotFound();

    // Helper：設定 CORS + JSON 回應
    void _sendJson(int code, const String &json);
    // Helper：取得客戶端 IP（作為 clientId）
    String _clientId();
};

extern HttpServer httpSrv;
