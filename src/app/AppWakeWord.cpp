#include <Arduino.h>
#include <M5Unified.h>

#include "app/AppWakeWord.h"
#include "app/lang.h"

void AppWakeWord::setup() {
    _detector.loadTemplate();
}

void AppWakeWord::start() {
    xTaskCreatePinnedToCore(
            [](void *arg) {
                auto *self = (AppWakeWord *) arg;
#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
                while (true) {
                    self->_loop();
                }
#pragma clang diagnostic pop
            },
            "AppWakeWord",
            8192,
            this,
            1,
            &_taskHandle,
            APP_CPU_NUM
    );
}

const char *AppWakeWord::registerWav(const uint8_t *data, size_t size) {
    xSemaphoreTake(_lock, portMAX_DELAY);
    auto error = _detector.registerWav(data, size);
    xSemaphoreGive(_lock);
    return error;
}

int AppWakeWord::templateFrames() {
    xSemaphoreTake(_lock, portMAX_DELAY);
    auto result = _detector.templateFrames();
    xSemaphoreGive(_lock);
    return result;
}

void AppWakeWord::_startMic() {
    if (_micActive) return;
    M5.Speaker.end();
    M5.Mic.begin();
    _micActive = true;
}

void AppWakeWord::_stopMic() {
    if (!_micActive) return;
    M5.Mic.end();
    M5.Speaker.begin();
    _micActive = false;
}

void AppWakeWord::_loop() {
    if (_resetFaceAt != 0 && millis() > _resetFaceAt) {
        _face->setExpression(Expression::Neutral);
        _resetFaceAt = 0;
    }

    // 発話中 (または発話予定) はマイクを止めてスピーカーに譲る
    if (!_settings->isWakeWordEnabled() || !_detector.hasTemplate() || _voice->isBusy()) {
        _stopMic();
        delay(200);
        return;
    }

    _startMic();
    static int16_t buf[WakeWordDetector::HOP_SIZE];
    if (M5.Mic.record(buf, WakeWordDetector::HOP_SIZE, WakeWordDetector::SAMPLE_RATE)) {
        while (M5.Mic.isRecording()) { delay(1); }
        xSemaphoreTake(_lock, portMAX_DELAY);
        int distance = _detector.feed(buf);
        xSemaphoreGive(_lock);
        if (distance >= 0) {
            Serial.printf("wakeword: distance=%d (threshold=%d)\n",
                          distance, _settings->getWakeWordThreshold());
            if (distance <= _settings->getWakeWordThreshold()) {
                _onDetect();
            }
        }
    } else {
        delay(10);
    }
}

void AppWakeWord::_onDetect() {
    Serial.println("wakeword: detected");
    _detector.reset();
    _stopMic();
    _face->setExpression(Expression::Happy);
    _voice->speak(t(_settings->getLang().c_str(), "wakeword_reply"), "");
    _resetFaceAt = millis() + 3000;
}
