#include <algorithm>
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

const char *AppWakeWord::registerWav(const uint8_t *data, size_t size, bool append) {
    xSemaphoreTake(_lock, portMAX_DELAY);
    auto error = _detector.registerWav(data, size, append);
    xSemaphoreGive(_lock);
    return error;
}

int AppWakeWord::micLevel() {
    if (!_micActive) return -1;
    xSemaphoreTake(_lock, portMAX_DELAY);
    auto result = (int) _detector.lastRms();
    xSemaphoreGive(_lock);
    return result;
}

int AppWakeWord::lastDistance() {
    if (_lastDistanceAt == 0 || millis() - _lastDistanceAt > 10000) return -1;
    return _lastDistance;
}

bool AppWakeWord::recordClip(int16_t *buf, size_t numSamples) {
    _captureBuf = buf;
    _captureLen = numSamples;
    _capturePos = 0;
    _captureDone = false;
    _captureRequested = true;
    // 録音時間 + 余裕分だけ完了を待つ
    unsigned long timeout = millis() + numSamples * 1000 / WakeWordDetector::SAMPLE_RATE + 5000;
    while (!_captureDone && millis() < timeout) { delay(50); }
    _captureRequested = false;
    return _captureDone;
}

int AppWakeWord::templateWords() {
    xSemaphoreTake(_lock, portMAX_DELAY);
    auto result = _detector.templateWords();
    xSemaphoreGive(_lock);
    return result;
}

void AppWakeWord::_startMic() {
    if (_micActive) return;
    M5.Speaker.end();
    auto cfg = M5.Mic.config();
    cfg.magnification = _settings->getWakeWordMicGain();
    // 外付け PDM マイクユニットが設定されていれば差し替える
    int clk = _settings->getWakeWordMicPinClk();
    int data = _settings->getWakeWordMicPinData();
    if (clk >= 0 && data >= 0) {
        cfg.pin_ws = clk;
        cfg.pin_data_in = data;
    }
    M5.Mic.config(cfg);
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
    bool wantDetect = _settings->isWakeWordEnabled() && _detector.hasTemplate() && !_voice->isBusy();
    bool capturing = _captureRequested;
    if (!wantDetect && !capturing) {
        _stopMic();
        delay(200);
        return;
    }

    _startMic();
    static int16_t buf[WakeWordDetector::HOP_SIZE];
    if (M5.Mic.record(buf, WakeWordDetector::HOP_SIZE, WakeWordDetector::SAMPLE_RATE)) {
        while (M5.Mic.isRecording()) { delay(1); }
        if (capturing) {
            // 録音要求中は照合せず、要求元のバッファへ書き溜める
            size_t n = std::min((size_t) WakeWordDetector::HOP_SIZE, _captureLen - _capturePos);
            memcpy(_captureBuf + _capturePos, buf, n * sizeof(int16_t));
            _capturePos += n;
            if (_capturePos >= _captureLen) {
                _captureDone = true;
            }
            return;
        }
        xSemaphoreTake(_lock, portMAX_DELAY);
        int distance = _detector.feed(buf);
        auto rms = _detector.lastRms();
        xSemaphoreGive(_lock);
        // マイクが生きているか確認できるよう、5秒ごとに区間最大音量を出す
        _maxRms = std::max(_maxRms, rms);
        auto now = millis();
        if (now - _lastLevelLog >= 5000) {
            Serial.printf("wakeword: mic level=%d\n", (int) _maxRms);
            _lastLevelLog = now;
            _lastPeak = _maxRms;
            _maxRms = 0;
        }
        if (distance >= 0) {
            _lastDistance = distance;
            _lastDistanceAt = now;
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
