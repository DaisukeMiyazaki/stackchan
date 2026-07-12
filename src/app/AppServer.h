#if !defined(APP_SERVER_H)
#define APP_SERVER_H

#include <ESP32WebServer.h>

#include <utility>

#include "app/AppChat.h"
#include "app/AppFace.h"
#include "app/AppIr.h"
#include "app/AppSettings.h"
#include "app/AppVoice.h"
#include "app/AppWakeWord.h"

class AppServer {
public:
    explicit AppServer(
            std::shared_ptr<AppSettings> settings,
            std::shared_ptr<AppVoice> voice,
            std::shared_ptr<AppFace> face,
            std::shared_ptr<AppChat> chat,
            std::shared_ptr<AppIr> ir,
            std::shared_ptr<AppWakeWord> wakeWord
    ) : _settings(std::move(settings)),
        _voice(std::move(voice)),
        _face(std::move(face)),
        _chat(std::move(chat)),
        _ir(std::move(ir)),
        _wakeWord(std::move(wakeWord)) {};

    void setup();

    void loop();

private:
    std::shared_ptr<AppSettings> _settings;
    std::shared_ptr<AppVoice> _voice;
    std::shared_ptr<AppFace> _face;
    std::shared_ptr<AppChat> _chat;
    std::shared_ptr<AppIr> _ir;
    std::shared_ptr<AppWakeWord> _wakeWord;

    ESP32WebServer _httpServer{80};

    /// upload buffer for wakeword WAV
    std::vector<uint8_t> _wavUpload;
    bool _wavUploadOverflow = false;

    /// Busy flag
    bool _busy = false;

    void _onRoot();

    void _onSpeech();

    void _onFace();

    void _onChat();

    void _onApikey();

    void _onApikeySet();

    void _onRoleGet();

    void _onRoleSet();

    void _onSetting();

    void _onSettings();

    void _onPlay();

    void _onIrLearn();

    void _onIrSend();

    void _onIrCodes();

    void _onWakeWordStatus();

    void _onWakeWordRegister();

    void _onWakeWordUpload();

    void _onNotFound();
};

#endif // !defined(APP_SERVER_H)
