package com.iot.esp32

import com.google.gson.annotations.SerializedName

// /role 回傳
data class RoleResponse(
    @SerializedName("role")        val role:       String  = "unknown",
    @SerializedName("key1_light")  val key1Light:  String  = "red",
    @SerializedName("key2_light")  val key2Light:  String  = "red",
    @SerializedName("can_operate") val canOperate: Boolean = false,
    @SerializedName("can_config")  val canConfig:  Boolean = false,
    @SerializedName("is_master")   val isMaster:   Boolean = false,
    @SerializedName("has_master")  val hasMaster:  Boolean = false
)

// /status 回傳
data class StatusResponse(
    @SerializedName("green")       val green:      Int = 0,
    @SerializedName("yellow")      val yellow:     Int = 0,
    @SerializedName("red")         val red:        Int = 0,
    @SerializedName("yellow_hold") val yellowHold: Int = 0
)

// /control 回傳
data class ControlResponse(
    @SerializedName("cmd")    val cmd:    String = "",
    @SerializedName("result") val result: String = ""
)

// /config 回傳
data class ConfigResponse(
    @SerializedName("result")       val result:      String  = "",
    @SerializedName("is_master")    val isMaster:    Boolean = false,
    @SerializedName("has_master")   val hasMaster:   Boolean = false,
    @SerializedName("current_ssid") val currentSsid: String  = "",
    @SerializedName("ap_restart")   val apRestart:   String  = "",
    @SerializedName("error")        val error:       String  = "",
    @SerializedName("reason")       val reason:      String  = ""
)
