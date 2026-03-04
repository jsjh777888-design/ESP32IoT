// ============================================================
//  gpio_control.cpp — GPIO 燈號控制與即時狀態管理
// ============================================================
#include "gpio_control.h"
#include "config.h"

GpioControl gpioCtrl;

GpioControl::GpioControl()
    : _greenOffTime(0), _yellowOffTime(0), _redOffTime(0),
      _yellowHold(false), _green(0), _yellow(0), _red(0)
{}

void GpioControl::begin() {
    pinMode(PIN_GREEN,  OUTPUT);
    pinMode(PIN_YELLOW, OUTPUT);
    pinMode(PIN_RED,    OUTPUT);
    digitalWrite(PIN_GREEN,  LOW);
    digitalWrite(PIN_YELLOW, LOW);
    digitalWrite(PIN_RED,    LOW);
    Serial.println("[GPIO] 初始化完成，三燈 LOW");
}

// ── 上電序列：GPIO35/37 LOW，GPIO36 HIGH 5 秒後 LOW ─────────
void GpioControl::startupSequence() {
    Serial.println("[GPIO] 上電序列：黃燈亮 5 秒...");
    digitalWrite(PIN_GREEN,  LOW);
    digitalWrite(PIN_RED,    LOW);
    digitalWrite(PIN_YELLOW, HIGH);
    _yellow = 1;
    delay(STARTUP_YELLOW_MS);
    digitalWrite(PIN_YELLOW, LOW);
    _yellow = 0;
    Serial.println("[GPIO] 上電序列完成");
}

// ── 指令 A：綠燈 1 秒 ────────────────────────────────────────
void GpioControl::cmdA() {
    _pulseGreen(GPIO_PULSE_MS);
    Serial.println("[GPIO] 指令A：綠燈 ON 1秒");
}

// ── 指令 B：黃燈 1 秒（取消 D 長亮）──────────────────────────
void GpioControl::cmdB() {
    _yellowHold = false;              // 取消長亮
    _pulseYellow(GPIO_PULSE_MS);
    Serial.println("[GPIO] 指令B：黃燈 ON 1秒（取消長亮）");
}

// ── 指令 C：紅燈 1 秒 ────────────────────────────────────────
void GpioControl::cmdC() {
    _pulseRed(GPIO_PULSE_MS);
    Serial.println("[GPIO] 指令C：紅燈 ON 1秒");
}

// ── 指令 D：黃燈長亮（直到 B 指令）──────────────────────────
void GpioControl::cmdD() {
    _yellowHold    = true;
    _yellowOffTime = 0;   // 0 = 不自動熄滅
    digitalWrite(PIN_YELLOW, HIGH);
    _yellow = 1;
    Serial.println("[GPIO] 指令D：黃燈長亮");
}

// ── update()：在 loop() 中呼叫，處理自動熄滅 ─────────────────
void GpioControl::update() {
    unsigned long now = millis();

    // 綠燈自動熄滅
    if (_greenOffTime > 0 && now >= _greenOffTime) {
        digitalWrite(PIN_GREEN, LOW);
        _green       = 0;
        _greenOffTime = 0;
    }

    // 黃燈自動熄滅（D 長亮時 _yellowOffTime = 0，不會熄滅）
    if (!_yellowHold && _yellowOffTime > 0 && now >= _yellowOffTime) {
        digitalWrite(PIN_YELLOW, LOW);
        _yellow       = 0;
        _yellowOffTime = 0;
    }

    // 紅燈自動熄滅
    if (_redOffTime > 0 && now >= _redOffTime) {
        digitalWrite(PIN_RED, LOW);
        _red        = 0;
        _redOffTime = 0;
    }
}

// ── 狀態查詢 ─────────────────────────────────────────────────
int  GpioControl::getGreen()      const { return _green;      }
int  GpioControl::getYellow()     const { return _yellow;     }
int  GpioControl::getRed()        const { return _red;        }
bool GpioControl::getYellowHold() const { return _yellowHold; }

// ── 私有：設定 Pulse ──────────────────────────────────────────
void GpioControl::_pulseGreen(unsigned long ms) {
    digitalWrite(PIN_GREEN, HIGH);
    _green        = 1;
    _greenOffTime = millis() + ms;
}

void GpioControl::_pulseYellow(unsigned long ms) {
    digitalWrite(PIN_YELLOW, HIGH);
    _yellow        = 1;
    _yellowOffTime = millis() + ms;
}

void GpioControl::_pulseRed(unsigned long ms) {
    digitalWrite(PIN_RED, HIGH);
    _red        = 1;
    _redOffTime = millis() + ms;
}
