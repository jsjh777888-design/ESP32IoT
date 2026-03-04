package com.iot.esp32

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import java.util.UUID

// ── 全頁 UI 狀態 ──────────────────────────────────────────────
data class UiState(
    // 輸入欄位
    val ip:       String = "192.168.4.1",
    val ssid:     String = "ESP32-IoT-Lab",
    val password: String = "12345678",
    val token:    String = "iot_token_123",
    val newSsid:  String = "",
    val newPass:  String = "",
    val newToken: String = "",

    // 確認鍵燈號："green" / "yellow" / "red"
    val key1Light: String = "red",
    val key2Light: String = "red",

    // 角色與權限
    val role:       String  = "unknown",
    val canOperate: Boolean = false,
    val canConfig:  Boolean = false,
    val isMaster:   Boolean = false,
    val hasMaster:  Boolean = false,

    // ESP32 燈號狀態
    val ledGreen:      Int     = 0,
    val ledYellow:     Int     = 0,
    val ledRed:        Int     = 0,
    val yellowHold:    Int     = 0,

    // 最後一次操作訊息
    val message:   String  = "",
    val isLoading: Boolean = false,
    val isPolling: Boolean = false
)

class MainViewModel(app: Application) : AndroidViewModel(app) {

    val prefs = Prefs(app)

    private val _ui = MutableStateFlow(UiState(
        ip       = prefs.ip,
        ssid     = prefs.ssid,
        password = prefs.password,
        token    = prefs.token
    ))
    val ui = _ui.asStateFlow()

    private val clientId: String get() {
        if (prefs.clientId.isEmpty()) prefs.clientId = UUID.randomUUID().toString().take(16)
        return prefs.clientId
    }

    private var pollingJob: Job? = null

    // ── 輸入欄位更新 ─────────────────────────────────────────
    fun setIp(v: String)       = _ui.update { it.copy(ip = v) }
    fun setSsid(v: String)     = _ui.update { it.copy(ssid = v) }
    fun setPassword(v: String) = _ui.update { it.copy(password = v) }
    fun setToken(v: String)    = _ui.update { it.copy(token = v) }
    fun setNewSsid(v: String)  = _ui.update { it.copy(newSsid = v) }
    fun setNewPass(v: String)  = _ui.update { it.copy(newPass = v) }
    fun setNewToken(v: String) = _ui.update { it.copy(newToken = v) }

    // ════════════════════════════════════════════════════════
    //  確認鍵 1 — SSID + 密碼確認
    //  按下時呼叫 /role，更新燈號與權限狀態
    //  若 canConfig，同時儲存輸入值並更新 Prefs
    // ════════════════════════════════════════════════════════
    fun confirmKey1() {
        val s = _ui.value
        _ui.update { it.copy(isLoading = true, message = "") }
        viewModelScope.launch {
            try {
                val resp = ApiClient.getApi(s.ip).getRole(s.ssid, s.password, s.token, clientId)
                if (resp.isSuccessful) {
                    val r = resp.body()!!
                    // 依計畫書：key1Light 由 SSID+密碼 決定
                    prefs.ip       = s.ip
                    prefs.ssid     = s.ssid
                    prefs.password = s.password
                    prefs.token    = s.token
                    _ui.update { it.copy(
                        key1Light  = r.key1Light,
                        key2Light  = r.key2Light,
                        role       = r.role,
                        canOperate = r.canOperate,
                        canConfig  = r.canConfig,
                        isMaster   = r.isMaster,
                        hasMaster  = r.hasMaster,
                        isLoading  = false,
                        message    = roleMessage(r.role, r.key1Light, r.key2Light)
                    )}
                    // 確認成功後啟動 LED 輪詢
                    startPolling()
                } else {
                    _ui.update { it.copy(isLoading = false, message = "連線失敗：HTTP ${resp.code()}") }
                }
            } catch (e: Exception) {
                _ui.update { it.copy(isLoading = false, message = "無法連線：${e.message}") }
            }
        }
    }

    // ════════════════════════════════════════════════════════
    //  確認鍵 2 — 口令確認（同樣呼叫 /role）
    //  計畫書：key2Light 由口令決定
    // ════════════════════════════════════════════════════════
    fun confirmKey2() = confirmKey1()  // 同一支 API，結果包含 key1 & key2

    // ════════════════════════════════════════════════════════
    //  按鍵 A / B / C / D
    // ════════════════════════════════════════════════════════
    fun sendCmd(cmd: String) {
        val s = _ui.value
        if (!s.canOperate) {
            _ui.update { it.copy(message = "無操作權限（需綠燈）") }
            return
        }
        _ui.update { it.copy(isLoading = true, message = "") }
        viewModelScope.launch {
            try {
                val resp = ApiClient.getApi(s.ip)
                    .control(cmd, s.token, s.ssid, s.password)
                if (resp.isSuccessful) {
                    val r = resp.body()!!
                    val msg = if (r.result == "ok") "指令 $cmd 已送出" else "指令 $cmd：${r.result}"
                    _ui.update { it.copy(isLoading = false, message = msg) }
                } else {
                    _ui.update { it.copy(isLoading = false,
                        message = if (resp.code() == 403) "無操作權限" else "錯誤：${resp.code()}") }
                }
            } catch (e: Exception) {
                _ui.update { it.copy(isLoading = false, message = "連線失敗：${e.message}") }
            }
        }
    }

    // ════════════════════════════════════════════════════════
    //  設定更新（主控手機才會同步到 ESP32）
    // ════════════════════════════════════════════════════════
    fun applyConfig() {
        val s = _ui.value
        if (!s.canConfig) {
            _ui.update { it.copy(message = "無設定權限（需綠燈或黃燈）") }
            return
        }
        _ui.update { it.copy(isLoading = true, message = "") }
        viewModelScope.launch {
            try {
                val resp = ApiClient.getApi(s.ip).updateConfig(
                    ssid     = s.ssid,
                    pass     = s.password,
                    token    = s.token,
                    newSsid  = s.newSsid.ifBlank { s.ssid },
                    newPass  = s.newPass.ifBlank { s.password },
                    newToken = s.newToken.ifBlank { s.token },
                    clientId = clientId
                )
                if (resp.isSuccessful) {
                    val r = resp.body()!!
                    when (r.result) {
                        "ok" -> {
                            // 若為主控手機，本機設定也要跟著更新
                            if (r.isMaster) {
                                if (s.newSsid.isNotBlank())  { prefs.ssid     = s.newSsid;  _ui.update { it.copy(ssid = s.newSsid) } }
                                if (s.newPass.isNotBlank())  { prefs.password = s.newPass;  _ui.update { it.copy(password = s.newPass) } }
                                if (s.newToken.isNotBlank()) { prefs.token    = s.newToken; _ui.update { it.copy(token = s.newToken) } }
                            }
                            _ui.update { it.copy(
                                isLoading = false,
                                newSsid   = "",
                                newPass   = "",
                                newToken  = "",
                                message   = if (r.isMaster) "✅ ESP32 設定已更新（主控手機）\nAP 重建中，請稍後重新確認…"
                                            else "✅ 本機設定已更新（從屬手機）",
                                isMaster  = r.isMaster,
                                hasMaster = r.hasMaster
                            )}
                        }
                        "local_only" ->
                            _ui.update { it.copy(isLoading = false,
                                message = "黃燈：僅更新本機設定，不同步到 ESP32") }
                        else ->
                            _ui.update { it.copy(isLoading = false,
                                message = "失敗：${r.error} ${r.reason}") }
                    }
                } else {
                    val code = resp.code()
                    val msg = when (code) {
                        403 -> "設定被拒絕（已有其他主控手機）"
                        500 -> "ESP32 NVS 寫入失敗"
                        else -> "錯誤：$code"
                    }
                    _ui.update { it.copy(isLoading = false, message = msg) }
                }
            } catch (e: Exception) {
                _ui.update { it.copy(isLoading = false, message = "連線失敗：${e.message}") }
            }
        }
    }

    // ════════════════════════════════════════════════════════
    //  LED 狀態輪詢（每 1 秒讀一次 /status）
    // ════════════════════════════════════════════════════════
    fun startPolling() {
        if (pollingJob?.isActive == true) return
        _ui.update { it.copy(isPolling = true) }
        pollingJob = viewModelScope.launch {
            while (true) {
                try {
                    val resp = ApiClient.getApi(_ui.value.ip).getStatus()
                    if (resp.isSuccessful) {
                        val s = resp.body()!!
                        _ui.update { it.copy(
                            ledGreen   = s.green,
                            ledYellow  = s.yellow,
                            ledRed     = s.red,
                            yellowHold = s.yellowHold
                        )}
                    }
                } catch (_: Exception) {}
                delay(1000)
            }
        }
    }

    fun stopPolling() {
        pollingJob?.cancel()
        _ui.update { it.copy(isPolling = false) }
    }

    override fun onCleared() {
        super.onCleared()
        stopPolling()
    }

    // ── 工具函式 ─────────────────────────────────────────────
    private fun roleMessage(role: String, k1: String, k2: String): String {
        val roleStr = when (role) {
            "master"     -> "主控手機"
            "slave_ok"   -> "從屬手機（綠燈）"
            "slave_diff" -> "從屬手機（黃燈）"
            "denied"     -> "拒絕（紅燈）"
            else         -> "未知"
        }
        return "角色：$roleStr  確認鍵1：${lightLabel(k1)}  確認鍵2：${lightLabel(k2)}"
    }

    private fun lightLabel(l: String) = when (l) {
        "green"  -> "🟢"
        "yellow" -> "🟡"
        else     -> "🔴"
    }
}
