#include <algorithm>
#include <Arduino.h>
#include <ESP32WebServer.h>

#include "app/AppChat.h"
#include "app/AppFace.h"
#include "app/AppIr.h"
#include "app/AppServer.h"
#include "app/AppVoice.h"
#include "lib/utils.h"

void AppServer::setup() {
    static const char *headerKeys[] = {"Content-Type"};
    _httpServer.collectHeaders(headerKeys, 1);
    _httpServer.on("/", [&] { _onRoot(); });
    _httpServer.on("/speech", [&] { _onSpeech(); });
    _httpServer.on("/face", [&] { _onFace(); });
    _httpServer.on("/chat", [&] { _onChat(); });
    _httpServer.on("/apikey", HTTP_GET, [&] { _onApikey(); });
    _httpServer.on("/apikey_set", HTTP_POST, [&] { _onApikeySet(); });
    _httpServer.on("/role_get", HTTP_GET, [&] { _onRoleGet(); });
    _httpServer.on("/role_set", HTTP_POST, [&] { _onRoleSet(); });
    _httpServer.on("/setting", [&] { _onSetting(); });
    _httpServer.on("/settings", [&] { _onSettings(); });
    _httpServer.on("/play", [&] { _onPlay(); });
    _httpServer.on("/ir/learn", [&] { _onIrLearn(); });
    _httpServer.on("/ir/send", [&] { _onIrSend(); });
    _httpServer.on("/ir/codes", HTTP_GET, [&] { _onIrCodes(); });
    _httpServer.on("/wakeword", HTTP_GET, [&] { _onWakeWordStatus(); });
    _httpServer.on("/wakeword/rec", HTTP_GET, [&] { _onWakeWordRec(); });
    _httpServer.on("/wakeword/register", HTTP_POST,
                   [&] { _onWakeWordRegister(); }, [&] { _onWakeWordUpload(); });
    _httpServer.onNotFound([&] { _onNotFound(); });
    _httpServer.begin();
}

void AppServer::loop() {
    if (!_busy) {
        _httpServer.handleClient();
    }
}

void AppServer::_onRoot() {
    _httpServer.send(200, "text/plain", "Hello, I'm Stack-chan!");
}

void AppServer::_onSpeech() {
    auto message = _httpServer.arg("say");
    auto expressionStr = _httpServer.arg("expression");
    auto voice = _httpServer.arg("voice");
    if (!_face->setExpression((Expression) expressionStr.toInt())) {
        _httpServer.send(400);
    }
    _voice->stopSpeak();
    _voice->speak(message, voice);
    _httpServer.send(200, "text/plain", "OK");
}

void AppServer::_onFace() {
    auto expressionStr = _httpServer.arg("expression");
    if (!_face->setExpression((Expression) expressionStr.toInt())) {
        _httpServer.send(400);
    }
    _httpServer.send(200, "text/plain", "OK");
}

void AppServer::_onChat() {
    auto text = _httpServer.arg("text");
    auto voiceName = _httpServer.arg("voice");
    _voice->stopSpeak();
    _chat->talk(text, voiceName, true, [&](const char *answer) {
        _httpServer.send(200, "text/plain", answer);
        _busy = false;
    });
    _busy = true;
}

void AppServer::_onApikey() {
    _httpServer.send(200, "text/plain", "OK");
}

void AppServer::_onApikeySet() {
    auto openAiApiKey = _httpServer.arg("openai");
    auto voiceTextApiKey = _httpServer.arg("voicetext");
    auto voicevoxApiKey = _httpServer.arg("voicevox");
    _settings->setOpenAiApiKey(openAiApiKey);
    _settings->setVoiceTextApiKey(voiceTextApiKey);
    _settings->setTtsQuestVoicevoxApiKey(voicevoxApiKey);
    if (!voicevoxApiKey.isEmpty()) {
        _settings->setVoiceService(VOICE_SERVICE_TTS_QUEST_VOICEVOX);
    } else if (!voiceTextApiKey.isEmpty()) {
        _settings->setVoiceService(VOICE_SERVICE_VOICETEXT);
    } else {
        _settings->setVoiceService(VOICE_SERVICE_GOOGLE_TRANSLATE_TTS);
    }
    _httpServer.send(200, "text/plain", "OK");
}

void AppServer::_onRoleGet() {
    JsonDocument result;
    result["roles"].to<JsonArray>();
    for (const auto &role: _settings->getChatRoles()) {
        result["roles"].add(role);
    }
    _httpServer.send(200, "application/json", jsonEncode(result));
}

void AppServer::_onRoleSet() {
    auto roleStr = _httpServer.arg("plain");
    bool result;
    if (roleStr == "") {
        result = _settings->clearRoles();
    } else {
        result = _settings->addRole(roleStr);
    }
    if (!result) {
        _httpServer.send(400);
    } else {
        _httpServer.send(200, "text/plain", "OK");
    }
}

void AppServer::_onSetting() {
    auto volumeStr = _httpServer.arg("volume");
    auto voiceName = _httpServer.arg("voice");
    if (volumeStr != "") {
        if (!_settings->setVoiceVolume(volumeStr.toInt())) {
            _httpServer.send(400);
        }
    }
    if (voiceName != "") {
        if (!_voice->setVoiceName(voiceName)) {
            _httpServer.send(400);
        }
    }
    _httpServer.send(200, "text/plain", "OK");
}

void AppServer::_onSettings() {
    bool result = true;
    if (_httpServer.method() == HTTPMethod::HTTP_POST ||
        _httpServer.method() == HTTPMethod::HTTP_PUT) {
        if (_httpServer.header("Content-Type").startsWith("application/json")) {
            result = _settings->load(_httpServer.arg("plain"),
                                     _httpServer.method() == HTTPMethod::HTTP_PUT);
        } else {
            for (int i = 0; i < _httpServer.args(); i++) {
                auto name = _httpServer.argName(i);
                auto val = _httpServer.arg(i);
                if (val == "") {
                    _settings->remove(name);
                } else if (std::all_of(val.begin(), val.end(), ::isdigit)) {
                    _settings->set(name, std::stoi(val.c_str()));
                } else if (val == "true") {
                    _settings->set(name, true);
                } else if (val == "false") {
                    _settings->set(name, false);
                } else {
                    _settings->set(name, val);
                }
            }
        }
    }
    auto settings = jsonEncode(_settings->get(""));
    if (!result) {
        _httpServer.send(400);
    } else {
        _httpServer.send(200, "application/json", settings);
    }
}

void AppServer::_onPlay() {
    auto url = _httpServer.arg("url");
    if (url.isEmpty()) {
        _httpServer.send(400, "text/plain", "url required");
        return;
    }
    _voice->stopSpeak();
    _voice->playUrl(url);
    _httpServer.send(200, "text/plain", "OK");
}

void AppServer::_onIrLearn() {
    auto name = _httpServer.arg("name");
    if (name.isEmpty()) {
        _httpServer.send(400, "text/plain", "name required");
        return;
    }
    // 学習待ち中はリモコンをユニットに向けてボタンを押してもらう
    if (_ir->learn(name)) {
        _httpServer.send(200, "text/plain", "OK");
    } else {
        _httpServer.send(408, "text/plain", "no IR signal received");
    }
}

void AppServer::_onIrSend() {
    auto name = _httpServer.arg("name");
    if (_ir->send(name)) {
        _httpServer.send(200, "text/plain", "OK");
    } else {
        _httpServer.send(404, "text/plain", "unknown code name");
    }
}

void AppServer::_onIrCodes() {
    JsonDocument result;
    result["codes"].to<JsonArray>();
    for (const auto &name: _ir->codeNames()) {
        result["codes"].add(name);
    }
    _httpServer.send(200, "application/json", jsonEncode(result));
}

void AppServer::_onWakeWordStatus() {
    JsonDocument result;
    result["enabled"] = _settings->isWakeWordEnabled();
    result["threshold"] = _settings->getWakeWordThreshold();
    result["words"] = _wakeWord->templateWords();
    result["micLevel"] = _wakeWord->micLevel();
    result["micPeak"] = _wakeWord->micPeak();
    result["distance"] = _wakeWord->lastDistance();
    _httpServer.send(200, "application/json", jsonEncode(result));
}

/// 登録用 WAV の最大サイズ (16kHz/16bit/mono で約12秒)
static const size_t WAV_UPLOAD_MAX = 400 * 1024;

void AppServer::_onWakeWordUpload() {
    auto &upload = _httpServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
        _wavUpload.clear();
        _wavUploadOverflow = false;
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (_wavUpload.size() + upload.currentSize <= WAV_UPLOAD_MAX) {
            _wavUpload.insert(_wavUpload.end(), upload.buf, upload.buf + upload.currentSize);
        } else {
            _wavUploadOverflow = true;
        }
    }
}

void AppServer::_onWakeWordRegister() {
    if (_wavUpload.empty()) {
        _httpServer.send(400, "text/plain", "WAV file required (multipart/form-data)");
        return;
    }
    if (_wavUploadOverflow) {
        _wavUpload.clear();
        _wavUpload.shrink_to_fit();
        _httpServer.send(400, "text/plain", "file too large");
        return;
    }
    bool append = _httpServer.arg("append") == "1" || _httpServer.arg("append") == "true";
    auto error = _wakeWord->registerWav(_wavUpload.data(), _wavUpload.size(), append);
    _wavUpload.clear();
    _wavUpload.shrink_to_fit();
    if (error != nullptr) {
        _httpServer.send(400, "text/plain", error);
    } else {
        _httpServer.send(200, "text/plain", "OK");
    }
}

/// 本体マイクで録音して WAV (16kHz/16bit/mono) を返す
void AppServer::_onWakeWordRec() {
    int ms = _httpServer.arg("ms").toInt();
    if (ms <= 0) ms = 2500;
    ms = std::min(ms, 10000);
    size_t numSamples = (size_t) 16000 * ms / 1000;
    size_t dataSize = numSamples * sizeof(int16_t);
    auto *buf = (int16_t *) ps_malloc(dataSize);
    if (buf == nullptr) {
        _httpServer.send(503, "text/plain", "out of memory");
        return;
    }
    if (!_wakeWord->recordClip(buf, numSamples)) {
        free(buf);
        _httpServer.send(503, "text/plain", "recording failed");
        return;
    }
    uint8_t header[44];
    memcpy(header, "RIFF", 4);
    uint32_t v32 = 36 + dataSize;
    memcpy(header + 4, &v32, 4);
    memcpy(header + 8, "WAVEfmt ", 8);
    v32 = 16;
    memcpy(header + 16, &v32, 4);
    uint16_t v16 = 1; // PCM
    memcpy(header + 20, &v16, 2);
    v16 = 1; // mono
    memcpy(header + 22, &v16, 2);
    v32 = 16000;
    memcpy(header + 24, &v32, 4);
    v32 = 16000 * 2;
    memcpy(header + 28, &v32, 4);
    v16 = 2;
    memcpy(header + 32, &v16, 2);
    v16 = 16;
    memcpy(header + 34, &v16, 2);
    memcpy(header + 36, "data", 4);
    v32 = dataSize;
    memcpy(header + 40, &v32, 4);
    _httpServer.setContentLength(sizeof(header) + dataSize);
    _httpServer.send(200, "audio/wav", "");
    _httpServer.sendContent_P((PGM_P) header, sizeof(header));
    _httpServer.sendContent_P((PGM_P) buf, dataSize);
    free(buf);
}

void AppServer::_onNotFound() {
    _httpServer.send(404);
}
