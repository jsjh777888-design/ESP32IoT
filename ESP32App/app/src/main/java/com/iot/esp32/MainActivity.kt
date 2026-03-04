package com.iot.esp32

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.viewModels
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

// ── 顏色定義 ─────────────────────────────────────────────────
private val ColorGreen   = Color(0xFF2E7D32)
private val ColorYellow  = Color(0xFFF9A825)
private val ColorRed     = Color(0xFFC62828)
private val ColorGrey    = Color(0xFFBDBDBD)
private val ColorBorder  = Color(0xFF1565C0)    // 計畫書藍色邊框
private val ColorBg      = Color(0xFFF5F5F5)
private val ColorTitle   = Color(0xFFC62828)    // 標題紅色（同計畫書）

fun lightColor(light: String, active: Boolean = true): Color = when {
    light == "green"  && active -> ColorGreen
    light == "yellow" && active -> ColorYellow
    light == "red"    && active -> ColorRed
    else                        -> ColorGrey
}

class MainActivity : ComponentActivity() {
    private val vm: MainViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme {
                MainScreen(vm)
            }
        }
    }
}

@Composable
fun MainScreen(vm: MainViewModel) {
    val ui by vm.ui.collectAsState()
    val scroll = rememberScrollState()

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(ColorBg)
            .verticalScroll(scroll)
            .padding(horizontal = 16.dp, vertical = 12.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        // ── ESP32 IP 設定（小字，置頂）────────────────────────
        IpRow(ip = ui.ip, onIpChange = vm::setIp)

        Divider(color = Color(0xFFE0E0E0))

        // ── 區塊 1：SSID / 密碼 + 確認鍵1 ────────────────────
        SectionCard {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.Bottom,
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Column(modifier = Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    FieldLabel("SSID")
                    OutlinedInput(
                        value    = ui.ssid,
                        onValue  = vm::setSsid,
                        hint     = "ESP32-IoT-Lab",
                        borderColor = lightColor(ui.key1Light)
                    )
                    FieldLabel("密碼：")
                    OutlinedInput(
                        value    = ui.password,
                        onValue  = vm::setPassword,
                        hint     = "12345678",
                        isPassword = true,
                        borderColor = lightColor(ui.key1Light)
                    )
                }
                // 確認鍵1 + 燈號
                Column(
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    ConfirmButton(label = "確認鍵1", onClick = vm::confirmKey1)
                    LightDot(color = lightColor(ui.key1Light))
                }
            }
        }

        // ── 區塊 2：新SSID / 新密碼 ──────────────────────────
        SectionCard {
            Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                FieldLabel("新SSID")
                OutlinedInput(
                    value   = ui.newSsid,
                    onValue = vm::setNewSsid,
                    hint    = "留空表示不修改"
                )
                FieldLabel("新密碼：")
                OutlinedInput(
                    value      = ui.newPass,
                    onValue    = vm::setNewPass,
                    hint       = "留空表示不修改",
                    isPassword = true
                )
            }
        }

        // ── 區塊 3：口令 / 新口令 + 確認鍵2 ──────────────────
        SectionCard {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.Bottom,
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Column(modifier = Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    FieldLabel("使用者口令：")
                    OutlinedInput(
                        value       = ui.token,
                        onValue     = vm::setToken,
                        hint        = "iot_token_123",
                        borderColor = lightColor(ui.key2Light)
                    )
                    FieldLabel("新使用者口令：")
                    OutlinedInput(
                        value   = ui.newToken,
                        onValue = vm::setNewToken,
                        hint    = "留空表示不修改"
                    )
                }
                // 確認鍵2 + 燈號
                Column(
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    ConfirmButton(label = "確認鍵2", onClick = vm::confirmKey2)
                    LightDot(color = lightColor(ui.key2Light))
                }
            }
        }

        // ── 套用設定按鈕（僅在有設定更新時顯示）─────────────
        val hasNewConfig = ui.newSsid.isNotBlank() || ui.newPass.isNotBlank() || ui.newToken.isNotBlank()
        if (hasNewConfig) {
            Button(
                onClick  = vm::applyConfig,
                enabled  = ui.canConfig && !ui.isLoading,
                modifier = Modifier.fillMaxWidth(),
                colors   = ButtonDefaults.buttonColors(containerColor = ColorBorder)
            ) {
                Text("套用設定", color = Color.White, fontWeight = FontWeight.Bold)
            }
        }

        // ── 區塊 4：按鍵 A / B / C / D ────────────────────────
        SectionCard {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                listOf("A", "B", "C", "D").forEach { cmd ->
                    CmdButton(
                        label   = "按鍵$cmd",
                        enabled = ui.canOperate && !ui.isLoading,
                        color   = when (cmd) {
                            "A" -> ColorBorder
                            "B" -> ColorBorder
                            "C" -> ColorBorder
                            else -> ColorBorder
                        },
                        modifier = Modifier.weight(1f),
                        onClick  = { vm.sendCmd(cmd) }
                    )
                }
            }
        }

        // ── 區塊 5：ESP32 三色燈即時狀態 ──────────────────────
        SectionCard {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceEvenly,
                verticalAlignment = Alignment.CenterVertically
            ) {
                LightDot(color = if (ui.ledGreen  == 1) ColorGreen  else ColorGrey, size = 52.dp)
                LightDot(color = if (ui.ledYellow == 1 || ui.yellowHold == 1) ColorYellow else ColorGrey, size = 52.dp)
                LightDot(color = if (ui.ledRed    == 1) ColorRed    else ColorGrey, size = 52.dp)
            }
            if (ui.isPolling) {
                Text(
                    text = "● 即時更新中",
                    fontSize = 11.sp,
                    color = ColorGreen,
                    modifier = Modifier.align(Alignment.CenterHorizontally)
                )
            }
        }

        // ── 訊息列 ────────────────────────────────────────────
        if (ui.message.isNotBlank()) {
            Text(
                text       = ui.message,
                fontSize   = 13.sp,
                color      = if (ui.message.startsWith("✅")) ColorGreen
                             else if (ui.message.startsWith("無") || ui.message.startsWith("失敗") || ui.message.startsWith("錯誤") || ui.message.startsWith("無法")) ColorRed
                             else Color.DarkGray,
                modifier   = Modifier
                    .fillMaxWidth()
                    .background(Color.White, RoundedCornerShape(6.dp))
                    .padding(10.dp)
            )
        }

        // ── 載入指示 ──────────────────────────────────────────
        if (ui.isLoading) {
            LinearProgressIndicator(
                modifier = Modifier.fillMaxWidth(),
                color    = ColorBorder
            )
        }

        // ── 頁腳標題（同計畫書）──────────────────────────────
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .border(2.dp, ColorTitle, RoundedCornerShape(4.dp))
                .padding(vertical = 10.dp),
            contentAlignment = Alignment.Center
        ) {
            Text(
                text       = "IoT教學系統套件",
                fontSize   = 20.sp,
                fontWeight = FontWeight.Bold,
                color      = ColorTitle,
                textAlign  = TextAlign.Center
            )
        }

        // ── 角色狀態（小字，底部）────────────────────────────
        if (ui.role != "unknown") {
            val roleStr = when (ui.role) {
                "master"     -> "主控手機"
                "slave_ok"   -> "從屬手機"
                "slave_diff" -> "從屬手機（設定不同）"
                "denied"     -> "拒絕"
                else         -> ui.role
            }
            Text(
                text     = "角色：$roleStr" +
                           if (ui.isMaster) "（本機為主控）" else "",
                fontSize = 11.sp,
                color    = Color.Gray,
                modifier = Modifier.fillMaxWidth(),
                textAlign = TextAlign.Center
            )
        }

        Spacer(modifier = Modifier.height(16.dp))
    }
}

// ════════════════════════════════════════════════════════════
//  共用 UI 元件
// ════════════════════════════════════════════════════════════

@Composable
fun IpRow(ip: String, onIpChange: (String) -> Unit) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Text("ESP32 IP：", fontSize = 12.sp, color = Color.Gray)
        OutlinedTextField(
            value         = ip,
            onValueChange = onIpChange,
            modifier      = Modifier.weight(1f).height(48.dp),
            textStyle     = androidx.compose.ui.text.TextStyle(fontSize = 13.sp),
            singleLine    = true,
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Uri),
            placeholder   = { Text("192.168.4.1", fontSize = 12.sp) }
        )
    }
}

@Composable
fun SectionCard(content: @Composable ColumnScope.() -> Unit) {
    Card(
        modifier  = Modifier.fillMaxWidth(),
        colors    = CardDefaults.cardColors(containerColor = Color.White),
        elevation = CardDefaults.cardElevation(2.dp)
    ) {
        Column(
            modifier = Modifier.padding(12.dp),
            content  = content
        )
    }
}

@Composable
fun FieldLabel(text: String) {
    Text(
        text     = text,
        fontSize = 12.sp,
        color    = Color(0xFF1565C0),  // 計畫書藍色標籤
        fontWeight = FontWeight.Medium
    )
}

@Composable
fun OutlinedInput(
    value:       String,
    onValue:     (String) -> Unit,
    hint:        String         = "",
    isPassword:  Boolean        = false,
    borderColor: Color          = Color(0xFF1565C0)
) {
    OutlinedTextField(
        value         = value,
        onValueChange = onValue,
        modifier      = Modifier
            .fillMaxWidth()
            .height(52.dp),
        textStyle     = androidx.compose.ui.text.TextStyle(fontSize = 14.sp),
        singleLine    = true,
        placeholder   = { Text(hint, fontSize = 12.sp, color = Color.LightGray) },
        visualTransformation = if (isPassword) PasswordVisualTransformation() else VisualTransformation.None,
        colors        = OutlinedTextFieldDefaults.colors(
            focusedBorderColor   = borderColor,
            unfocusedBorderColor = borderColor.copy(alpha = 0.6f)
        )
    )
}

@Composable
fun ConfirmButton(label: String, onClick: () -> Unit) {
    OutlinedButton(
        onClick = onClick,
        modifier = Modifier.width(88.dp),
        border   = ButtonDefaults.outlinedButtonBorder.copy(
            width = 2.dp
        ),
        colors   = ButtonDefaults.outlinedButtonColors(contentColor = ColorBorder)
    ) {
        Text(label, fontSize = 12.sp, fontWeight = FontWeight.Bold, textAlign = TextAlign.Center)
    }
}

@Composable
fun LightDot(
    color:    Color,
    size:     androidx.compose.ui.unit.Dp = 32.dp
) {
    Box(
        modifier = Modifier
            .size(size)
            .clip(CircleShape)
            .background(color)
    )
}

@Composable
fun CmdButton(
    label:    String,
    enabled:  Boolean,
    color:    Color,
    modifier: Modifier = Modifier,
    onClick:  () -> Unit
) {
    OutlinedButton(
        onClick  = onClick,
        enabled  = enabled,
        modifier = modifier.height(44.dp),
        border   = ButtonDefaults.outlinedButtonBorder.copy(width = 2.dp),
        colors   = ButtonDefaults.outlinedButtonColors(
            contentColor         = color,
            disabledContentColor = Color.LightGray
        )
    ) {
        Text(label, fontSize = 14.sp, fontWeight = FontWeight.Bold)
    }
}
