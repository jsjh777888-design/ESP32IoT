package com.iot.esp32

import retrofit2.Response
import retrofit2.http.GET
import retrofit2.http.Query

interface Esp32Api {

    @GET("status")
    suspend fun getStatus(): Response<StatusResponse>

    @GET("control")
    suspend fun control(
        @Query("cmd")   cmd:   String,
        @Query("token") token: String,
        @Query("ssid")  ssid:  String = "",
        @Query("pass")  pass:  String = ""
    ): Response<ControlResponse>

    @GET("role")
    suspend fun getRole(
        @Query("ssid")     ssid:     String,
        @Query("pass")     pass:     String,
        @Query("token")    token:    String,
        @Query("clientId") clientId: String = ""
    ): Response<RoleResponse>

    @GET("config")
    suspend fun updateConfig(
        @Query("ssid")     ssid:     String,
        @Query("pass")     pass:     String,
        @Query("token")    token:    String,
        @Query("newSsid")  newSsid:  String = "",
        @Query("newPass")  newPass:  String = "",
        @Query("newToken") newToken: String = "",
        @Query("clientId") clientId: String = ""
    ): Response<ConfigResponse>
}
