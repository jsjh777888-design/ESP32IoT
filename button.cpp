// ============================================================
//  button.cpp — GPIO13 重置按鍵（非阻塞消抖 + 3 秒長按）
//  ★ 修正：移除 delay()，改用 millis() 非阻塞計時
//    避免 loop() 被卡住導致 Watchdog 觸發
// ============================================================
#include "button.h"
#include "config.h"

ResetButton resetBtn;

ResetButton::ResetButton()
    : _pressing(false), _longPressEvent(false),
      _pressStartMs(0), _relockUntilMs(0)
{}

void ResetButton::begin() {
    pinMode(PIN_RESET_BTN, INPUT_PULLUP); // 低電位 = 按下
    Serial.println("[Button] GPIO13 初始化（INPUT_PULLUP）");
}

// ── 完全非阻塞，不使用任何 delay() ───────────────────────────
void ResetButton::update() {
    unsigned long now = millis();

    // 鎖定期間不處理（重設後防重複觸發）
    if (now < _relockUntilMs) return;

    int level = digitalRead(PIN_RESET_BTN);

    if (level == LOW) {
        if (!_pressing) {
            // 第一次偵測到按下：記錄時間，下一輪確認消抖
            _pressing     = true;
            _pressStartMs = now;
        } else {
            // 已在按壓中：計算持按時間
            unsigned long held = now - _pressStartMs;

            // 消抖確認（held < 50ms 視為雜訊，忽略）
            if (held < BTN_DEBOUNCE_MS) return;

            // 長按判定（消抖後再等 3000ms）
            if (held >= (BTN_DEBOUNCE_MS + BTN_LONG_PRESS_MS)
                && !_longPressEvent) {
                _longPressEvent = true;
                _pressing       = false;
                _relockUntilMs  = now + BTN_RELOCK_MS;
                Serial.println("[Button] GPIO13 長按 3 秒，觸發重設事件");
            }
        }
    } else {
        // 按鍵放開，重設狀態
        _pressing = false;
    }
}

// ── 輪詢長按事件（讀取後自動清除旗標）───────────────────────
bool ResetButton::isLongPressed() {
    if (_longPressEvent) {
        _longPressEvent = false;
        return true;
    }
    return false;
}
