package com.iot.esp32

import okhttp3.OkHttpClient
import retrofit2.Retrofit
import retrofit2.converter.gson.GsonConverterFactory
import java.util.concurrent.TimeUnit

object ApiClient {
    private var baseIp = "192.168.4.1"
    private var retrofit: Retrofit? = null

    fun getApi(ip: String = baseIp): Esp32Api {
        if (ip != baseIp || retrofit == null) {
            baseIp = ip
            retrofit = Retrofit.Builder()
                .baseUrl("http://$ip/")
                .client(
                    OkHttpClient.Builder()
                        .connectTimeout(8, TimeUnit.SECONDS)
                        .readTimeout(8,    TimeUnit.SECONDS)
                        .writeTimeout(8,   TimeUnit.SECONDS)
                        .build()
                )
                .addConverterFactory(GsonConverterFactory.create())
                .build()
        }
        return retrofit!!.create(Esp32Api::class.java)
    }
}
