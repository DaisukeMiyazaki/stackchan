#if !defined(APP_APP_H)
#define APP_APP_H

#include <M5Unified.h>
#include <utility>

#include "app/AppChat.h"
#include "app/AppFace.h"
#include "app/AppIr.h"
#include "app/AppSettings.h"
#include "app/AppServer.h"
#include "app/AppVoice.h"

class App {
public:
    explicit App(
            std::shared_ptr<AppSettings> settings,
            std::shared_ptr<AppVoice> voice,
            std::shared_ptr<AppFace> face,
            std::shared_ptr<AppChat> chat,
            std::shared_ptr<AppServer> server,
            std::shared_ptr<AppIr> ir
    ) : _settings(std::move(settings)),
        _voice(std::move(voice)),
        _face(std::move(face)),
        _chat(std::move(chat)),
        _server(std::move(server)),
        _ir(std::move(ir)) {};

    void setup();

    void loop();

private:
    std::shared_ptr<AppSettings> _settings;
    std::shared_ptr<AppVoice> _voice;
    std::shared_ptr<AppFace> _face;
    std::shared_ptr<AppChat> _chat;
    std::shared_ptr<AppServer> _server;
    std::shared_ptr<AppIr> _ir;

    void _onButtonA();

    void _onButtonB();

    void _onButtonC();
};

#endif // !defined(APP_APP_H)
