#pragma once
// ============================================================
//  button.h — GPIO13 重置按鍵（消抖 + 長按偵測）
// ============================================================
#include <Arduino.h>

class ResetButton {
public:
    ResetButton();
    void begin();   // 初始化 GPIO13 為 INPUT_PULLUP
    void update();  // 在 loop() 中呼叫，回傳長按事件

    // 長按 3 秒後回呼一次（在 update() 中自動呼叫）
    // 外部透過 isLongPressed() 輪詢
    bool isLongPressed();

private:
    bool          _pressing;
    bool          _longPressEvent;  // 長按事件旗標（讀取後清除）
    unsigned long _pressStartMs;
    unsigned long _relockUntilMs;   // 重設後防重複觸發時間
};

extern ResetButton resetBtn;
