package com.iot.esp32

import android.content.Context

class Prefs(ctx: Context) {
    private val sp = ctx.getSharedPreferences("esp32", Context.MODE_PRIVATE)

    // 預設值對應計畫書：SSID / 密碼 / 口令 預設都是 123
    var ip:       String get() = sp.getString("ip",    "192.168.4.1")!! ; set(v) = sp.edit().putString("ip",    v).apply()
    var ssid:     String get() = sp.getString("ssid",  "ESP32-IoT-Lab")!!; set(v) = sp.edit().putString("ssid",  v).apply()
    var password: String get() = sp.getString("pass",  "12345678")!!     ; set(v) = sp.edit().putString("pass",  v).apply()
    var token:    String get() = sp.getString("token", "iot_token_123")!!; set(v) = sp.edit().putString("token", v).apply()
    var clientId: String get() = sp.getString("cid",   "")!!             ; set(v) = sp.edit().putString("cid",   v).apply()
}
