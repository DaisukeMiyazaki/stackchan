#include <memory>

#include "app/App.h"

static std::shared_ptr<App> app;

void setup() {
    auto settings = std::make_shared<AppSettings>();
    auto voice = std::make_shared<AppVoice>(settings);
    auto face = std::make_shared<AppFace>(settings, voice);
    auto chat = std::make_shared<AppChat>(settings, voice, face);
    auto ir = std::make_shared<AppIr>(settings);
    auto wakeWord = std::make_shared<AppWakeWord>(settings, voice, face);
    auto server = std::make_shared<AppServer>(settings, voice, face, chat, ir, wakeWord);
    app = std::make_shared<App>(settings, voice, face, chat, server, ir, wakeWord);
    app->setup();
}

void loop() {
    app->loop();
}
