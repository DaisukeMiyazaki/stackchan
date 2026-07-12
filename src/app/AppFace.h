#if !defined(APP_FACE_H)
#define APP_FACE_H

#if !defined(WITHOUT_AVATAR)
#include <Avatar.h>
#endif // !defined(WITHOUT_AVATAR)

#include "app/AppSettings.h"
#include "app/AppVoice.h"

typedef enum {
    Neutral = 0,
    Happy,
    Sleepy,
    Doubt,
    Sad,
    Angry,
} Expression;

class AppFace {
public:
    explicit AppFace(
            std::shared_ptr<AppSettings> settings,
            std::shared_ptr<AppVoice> voice
    ) : _settings(std::move(settings)),
        _voice(std::move(voice)) {};

    bool init();

    void setup();

    void start();

    void loop();

    void lipSync(void *args);

    void setText(const char *text);

    bool setExpression(Expression expression);

private:
    std::shared_ptr<AppSettings> _settings;
    std::shared_ptr<AppVoice> _voice;

#if !defined(WITHOUT_AVATAR)
    /// M5Stack-Avatar https://github.com/meganetaaan/m5stack-avatar
    m5avatar::Avatar _avatar;

    /// last time of get battery status
    unsigned long _lastBatteryStatus = 0;
#endif // !defined(WITHOUT_AVATAR)
};

#endif // !defined(APP_FACE_H)
