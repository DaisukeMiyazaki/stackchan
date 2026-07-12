#include <Arduino.h>

#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>

#include "app/AppIr.h"

/// Core2 の Port A (Grove) を想定した既定ピン
static const int DEFAULT_TX_PIN = 33;
static const int DEFAULT_RX_PIN = 32;

/// 送信キャリア周波数 (kHz)。家電リモコンはほぼ38kHz
static const uint16_t IR_FREQUENCY_KHZ = 38;

void AppIr::setup() {
    int txPin = _settings->has("ir.txPin") ? _settings->get("ir.txPin").as<int>() : DEFAULT_TX_PIN;
    int rxPin = _settings->has("ir.rxPin") ? _settings->get("ir.rxPin").as<int>() : DEFAULT_RX_PIN;
    _irSend = std::unique_ptr<IRsend>(new IRsend((uint16_t) txPin));
    _irSend->begin();
    _irRecv = std::unique_ptr<IRrecv>(new IRrecv((uint16_t) rxPin, 1024, 50, true));
    Serial.printf("IR: tx=%d rx=%d\n", txPin, rxPin);
}

bool AppIr::learn(const String &name, uint32_t timeoutMs) {
    if (!_irRecv || name.isEmpty()) {
        return false;
    }
    _irRecv->enableIRIn();
    decode_results results;
    auto start = millis();
    bool received = false;
    while (millis() - start < timeoutMs) {
        if (_irRecv->decode(&results) && !results.overflow) {
            received = true;
            break;
        }
        delay(10);
    }
    _irRecv->disableIRIn();
    if (!received) {
        return false;
    }

    // プロトコルに依存しないよう、生のパルス列(µs)をCSVで保存する
    auto length = getCorrectedRawLength(&results);
    auto *raw = resultToRawArray(&results);
    String csv;
    for (uint16_t i = 0; i < length; i++) {
        if (i > 0) csv += ",";
        csv += String(raw[i]);
    }
    delete[] raw;

    Serial.printf("IR: learned '%s' (%s, %d pulses)\n",
                  name.c_str(), typeToString(results.decode_type).c_str(), length);
    if (!_settings->set("ir.codes." + name, csv)) {
        return false;
    }
    return _settings->save();
}

bool AppIr::send(const String &name) {
    if (!_irSend || !_settings->has("ir.codes." + name)) {
        return false;
    }
    String csv = _settings->get("ir.codes." + name).as<String>();
    std::vector<uint16_t> pulses;
    int start = 0;
    while (start < (int) csv.length()) {
        int comma = csv.indexOf(',', start);
        if (comma < 0) comma = (int) csv.length();
        pulses.push_back((uint16_t) csv.substring(start, comma).toInt());
        start = comma + 1;
    }
    if (pulses.empty()) {
        return false;
    }
    _irSend->sendRaw(pulses.data(), pulses.size(), IR_FREQUENCY_KHZ);
    Serial.printf("IR: sent '%s' (%d pulses)\n", name.c_str(), pulses.size());
    return true;
}

std::vector<String> AppIr::codeNames() {
    std::vector<String> names;
    for (JsonPair kv: _settings->get("ir.codes").as<JsonObject>()) {
        names.emplace_back(kv.key().c_str());
    }
    return names;
}
